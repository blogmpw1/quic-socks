//
// Created by suun on 1/4/2022.
//

#include "tunnel/http_proxy.h"

#include <fmt/ostream.h>
#define SPDLOG_ACTIVE_LEVEL 0
#include <spdlog/spdlog.h>

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <memory>
#include <regex>
#include <variant>

#include "entities.h"
#include "tunnel/asio_coro.h"
#include "utility/result.h"

namespace socks::tunnel {

class Session {
 public:
  Session(asio::io_context &ctx, asio::ip::tcp::socket socket)
      : ctx_{ctx}, socket_{std::move(socket)}, remote_{ctx}, local_buf_{} {}

  void Start() noexcept {
    co_spawn(
        ctx_, [this] { return this->AsyncStart(); }, asio::detached);
  }

  asio::awaitable<void> AsyncStart() noexcept {
    RequestEntity entity;
    try {
      entity = co_await ParseRequest();
      const auto uri = Uri::Parse(entity.uri);
      co_await ConnectRemote(uri);

      if (entity.method == "CONNECT") {
        std::string response =
            fmt::format("HTTP/1.1 200 Connection Established\r\n\r\n");
        co_await socket_.async_write_some(asio::buffer(response),
                                          asio::use_awaitable);
      } else {
        const std::string request = entity.Dump();
        co_await remote_.async_write_some(asio::buffer(request),
                                          asio::use_awaitable);
      }

      co_await WaitAll(ctx_, std::chrono::hours{24},
                       ForwardTo(&socket_, &remote_),
                       ForwardTo(&remote_, &socket_));
    } catch (std::runtime_error &e) {
      SPDLOG_ERROR("[tunnel] close socket, uri={}, e={}", entity.uri, e.what());
    }
    Close();
  }

  static asio::awaitable<asio::system_error> ForwardTo(
      asio::ip::tcp::socket *from, asio::ip::tcp::socket *to) {
    std::array<char, 8196> buf{};
    while (true) {
      try {
        const auto len = co_await from->async_read_some(asio::buffer(buf),
                                                        asio::use_awaitable);
        SPDLOG_DEBUG("[tunnel] {} >-- {}, len={}", from->remote_endpoint(),
                     to->remote_endpoint(), len);
        co_await to->async_write_some(asio::buffer(buf, len),
                                      asio::use_awaitable);
        SPDLOG_DEBUG("[tunnel] {} --> {}, len={}", from->remote_endpoint(),
                     to->remote_endpoint(), len);
      } catch (asio::system_error &e) {
        co_return e;
      }
    }
  }

  asio::awaitable<void> ConnectRemote(const Uri &uri) {
    asio::error_code err;
    auto address = asio::ip::make_address(uri.host, err);
    if (err) {
      asio::ip::tcp::resolver resolver{ctx_};
      const auto resolve_res = co_await resolver.async_resolve(
          uri.host, std::to_string(uri.port), asio::use_awaitable);
      if (resolve_res.empty()) {
        throw SocksException(
            fmt::format("[tunnel] resolve dns failed, host={}", uri.host));
      }

      address = resolve_res->endpoint().address();
    }

    const auto ep = asio::ip::tcp::endpoint{address, uri.port};
    co_await remote_.async_connect(ep, asio::use_awaitable);
    SPDLOG_INFO("[tunnel] remote connected, uri={}, ep={}", uri, ep);
  }

  asio::awaitable<RequestEntity> ParseRequest() {
    asio::streambuf buf;
    std::istream stream{&buf};
    RequestEntity entity;
    // Request line
    co_await async_read_until(socket_, buf, "\r\n", asio::use_awaitable);
    stream >> entity.method >> entity.uri >> entity.ver;
    // Headers
    while (true) {
      co_await async_read_until(socket_, buf, "\r\n", asio::use_awaitable);
      std::string header_line;
      std::getline(stream, header_line);
      // Header part ends
      if (header_line.size() <= 2) break;

      // Trim "\r\n"
      std::string_view trimmed{header_line.c_str(), header_line.size() - 2};
      const auto delimiter = ": ";
      const auto pos = trimmed.find_first_of(delimiter);
      if (pos == std::string::npos) {
        throw SocksException{fmt::format(
            "[tunnel] parse request header failed, line={}", trimmed)};
      }

      std::string key{trimmed.substr({}, pos)}, value{trimmed.substr(pos + 2)};
      entity.headers.emplace(std::move(key), std::move(value));
    }
    SPDLOG_INFO("[tunnel] request parsed, entity={}", entity);
    co_return entity;
  }

  void Close() {
    if (socket_.is_open()) {
      SPDLOG_INFO("[tunnel] close socket, remote={}",
                  socket_.remote_endpoint());
      socket_.close();
    }
    if (remote_.is_open()) {
      remote_.close();
    }
  }

 private:
  asio::io_context &ctx_;
  asio::ip::tcp::socket socket_;
  asio::ip::tcp::socket remote_;

  constexpr static size_t kMaxBufLen = 1024;
  std::array<uint8_t, kMaxBufLen> local_buf_;
};

class HttpProxyImpl final : public HttpProxy {
 public:
  explicit HttpProxyImpl(const uint16_t port)
      : ctx_{ASIO_CONCURRENCY_HINT_UNSAFE_IO},
        acceptor_{ctx_, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), port}} {}
  ~HttpProxyImpl() override {
    if (thread_ != nullptr) {
      ctx_.stop();
      thread_->join();
    }
  }
  void Start() override {
    thread_ = std::make_unique<std::thread>([this] { ThreadRun(); });
  }

  void ThreadRun() {
    co_spawn(
        ctx_, [this] { return this->Accept(); }, asio::detached);
    ctx_.run();
  }

 private:
  asio::awaitable<void> Accept() {
    while (true) {
      try {
        auto socket = co_await acceptor_.async_accept(asio::use_awaitable);
        auto session = std::make_shared<Session>(ctx_, std::move(socket));
        session->Start();
        sessions_.push_back(std::move(session));
      } catch (const asio::system_error &e) {
        SPDLOG_ERROR("accept exception, e={}", e.what());
        break;
      }
    }
  }

  asio::io_context ctx_;
  asio::ip::tcp::acceptor acceptor_;
  std::unique_ptr<std::thread> thread_;
  std::vector<std::shared_ptr<Session>> sessions_;
};

std::shared_ptr<HttpProxy> HttpProxy::Create(uint16_t port) {
  return std::make_shared<HttpProxyImpl>(port);
}
}  // namespace socks::tunnel
