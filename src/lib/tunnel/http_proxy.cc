//
// Created by suun on 1/4/2022.
//

#include "tunnel/http_proxy.h"

#include <fmt/ostream.h>

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <memory>
#include <regex>
#include <variant>

#include "entities.h"
#include "tunnel/asio_helper.h"
#include "utility/log.h"
#include "utility/result.h"

namespace socks::tunnel {

class Session {
 public:
  Session(size_t idx, asio::io_context &ctx, asio::ip::tcp::socket socket)
      : idx_{idx}, ctx_{ctx}, socket_{std::move(socket)}, remote_{ctx} {}

  void Start() noexcept {
    co_spawn(
        ctx_, [this] { return this->AsyncStart(); }, asio::detached);
  }

  asio::awaitable<void> AsyncStart() noexcept {
    RequestEntity entity;
    try {
      asio::streambuf local_buf;
      entity = co_await ParseRequest(&local_buf);
      const auto uri = Uri::Parse(entity.uri);
      co_await ConnectRemote(uri);

      if (entity.method == "CONNECT") {
        const std::string response = {
            "HTTP/1.1 200 Connection Established\r\n\r\n"};
        co_await socket_.async_write_some(asio::buffer(response),
                                          asio::use_awaitable);
      } else {
        const std::string request = entity.Dump();
        co_await remote_.async_write_some(asio::buffer(request),
                                          asio::use_awaitable);
      }

      asio::streambuf conn_buf;
      co_await WaitAll(ctx_, std::chrono::hours{24},
                       ForwardTo(&local_buf, &socket_, &remote_),
                       ForwardTo(&conn_buf, &remote_, &socket_));
    } catch (std::runtime_error &e) {
      SPDLOG_ERROR("[tunnel] close socket, uri={}, e={}, idx={}", entity.uri,
                   e.what(), idx_);
    }
    Close();
  }

  asio::awaitable<asio::system_error> ForwardTo(asio::streambuf *buf,
                                                asio::ip::tcp::socket *from,
                                                asio::ip::tcp::socket *to) {
    while (true) {
      try {
        auto len = co_await from->async_read_some(buf->prepare(8196),
                                                  asio::use_awaitable);
        buf->commit(len);
        len = co_await to->async_write_some(buf->data(), asio::use_awaitable);
        buf->consume(len);
        SPDLOG_DEBUG("[tunnel] {} --> {}, len={}, idx={}",
                     from->remote_endpoint(), to->remote_endpoint(), len, idx_);
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
    SPDLOG_INFO("[tunnel] remote connected, uri={}, ep={}, idx={}", uri, ep,
                idx_);
  }

  asio::awaitable<RequestEntity> ParseRequest(asio::streambuf *buf) {
    while (true) {
      auto len = co_await socket_.async_read_some(buf->prepare(8196),
                                                  asio::use_awaitable);
      buf->commit(len);
      SPDLOG_DEBUG("[tunnel] {} |--, len={}, idx={}", socket_.remote_endpoint(),
                   len, idx_);
      if (len == 0) continue;

      std::string kRequestEnd = "\r\n\r\n";
      const auto const_buf = buf->data();
      const auto buf_begin = static_cast<const char *>(const_buf.data());
      const auto buf_end =
          static_cast<const char *>(const_buf.data()) + const_buf.size();
      const auto pos = std::search(buf_begin, buf_end, std::begin(kRequestEnd),
                                   std::end(kRequestEnd));
      if (pos != buf_end) {
        const auto raw_len =
            static_cast<size_t>(pos - buf_begin) + kRequestEnd.size();
        buf->consume(raw_len);
        auto res = RequestEntity::Parse(
            std::string_view{buf_begin, static_cast<size_t>(raw_len)});
        if (!res) {
          throw std::move(res.Error());
        }

        co_return res.Value();
      }
    }
  }

  void Close() {
    if (socket_.is_open()) {
      socket_.close();
    }
    if (remote_.is_open()) {
      remote_.close();
    }

    if (socket_.is_open() || remote_.is_open()) {
      SPDLOG_INFO("[tunnel] close socket, local={}, remote={}, idx={}",
                  socket_.remote_endpoint(), remote_.remote_endpoint(), idx_);
    }
  }

 private:
  size_t idx_;
  asio::io_context &ctx_;
  asio::ip::tcp::socket socket_;
  asio::ip::tcp::socket remote_;
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
    size_t idx{0};
    while (true) {
      try {
        auto socket = co_await acceptor_.async_accept(asio::use_awaitable);
        auto session =
            std::make_shared<Session>(idx++, ctx_, std::move(socket));
        asio::co_spawn(
            ctx_,
            [session]() -> asio::awaitable<void> {
              co_await session->AsyncStart();
            },
            asio::detached);
      } catch (const asio::system_error &e) {
        SPDLOG_ERROR("accept exception, e={}", e.what());
        break;
      }
    }
  }

  asio::io_context ctx_;
  asio::ip::tcp::acceptor acceptor_;
  std::unique_ptr<std::thread> thread_;
};

std::shared_ptr<HttpProxy> HttpProxy::Create(uint16_t port) {
  return std::make_shared<HttpProxyImpl>(port);
}
}  // namespace socks::tunnel
