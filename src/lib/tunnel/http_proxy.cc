//
// Created by suun on 1/4/2022.
//

#include "tunnel/http_proxy.h"

#include <spdlog/spdlog.h>

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <map>
#include <memory>
#include <ranges>
#include <regex>
#include <variant>

// ReSharper disable once CppUnusedIncludeDirective
#include "tunnel/asio_formatter.h"
#include "utility/result.h"

namespace socks::tunnel {

struct Uri {
  uint16_t port{};
  std::string scheme;
  std::string host;
  std::string path;

  static Uri Parse(std::string_view s) {
    const std::regex pattern{"^((\\w+)://)?([^/:]+)(:(\\d+))?(.*)?$"};
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_match(s.begin(), s.end(), match, pattern)) {
      throw SocksException(
          fmt::format("[tunnel] parse resource failed, s={}", s));
    }

    Uri uri;
    uri.scheme = match.str(2);
    uri.host = match.str(3);
    const auto port_str = match.str(5);
    if (port_str.empty()) {
      if (uri.scheme == "http") {
        uri.port = 80;
      } else if (uri.scheme == "https") {
        uri.port = 443;
      }
    } else {
      uri.port =
          static_cast<uint16_t>(std::strtol(port_str.c_str(), nullptr, 10));
    }
    uri.path = match.str(6);
    return uri;
  }
};

struct RequestEntity {
  std::string method;
  std::string host;
  std::string ver;
  std::multimap<std::string, std::string> headers;

  RequestEntity() = default;
};

class Session {
 public:
  Session(asio::io_context &ctx, asio::ip::tcp::socket socket)
      : ctx_{ctx}, socket_{std::move(socket)}, remote_{ctx}, local_buf_{} {}

  void Start() noexcept {
    co_spawn(
        ctx_, [this] { return this->AsyncStart(); }, asio::detached);
  }

  asio::awaitable<void> AsyncStart() noexcept {
    try {
      const auto entity = co_await ParseRequest();
      const auto uri = Uri::Parse(entity.host);
      co_await ConnectRemote(uri);
    } catch (std::runtime_error &e) {
      Close();
    }

    try {
      while (true) {
        co_await LoopOnce();
      }
    } catch (asio::system_error &e) {
      spdlog::info("[tunnel] session stops, e={}", e.what());
    }
  }

  asio::awaitable<void> ConnectRemote(
      const Uri &uri) {
    asio::error_code err;
    auto address = asio::ip::make_address(uri.host, err);
    if (err) {
      asio::ip::tcp::resolver resolver{ctx_};
      const auto resolve_res = co_await resolver.async_resolve(
          uri.host, std::to_string(uri.port), asio::use_awaitable);
      if (resolve_res.empty()) {
        spdlog::error("[tunnel] resolve dns failed, host={}", uri.host);
        throw SocksException()
      }

      address = resolve_res->endpoint().address();
    }

    const auto ep = asio::ip::tcp::endpoint{address, uri.port};
    co_await remote_.async_connect(ep, asio::use_awaitable);
  }

  asio::awaitable<RequestEntity> ParseRequest() {
    asio::streambuf buf;
    std::istream stream{&buf};
    RequestEntity entity;
    // Request line
    co_await async_read_until(socket_, buf, "\r\n", asio::use_awaitable);
    stream >> entity.method >> entity.host >> entity.ver;
    // Headers
    while (true) {
      co_await async_read_until(socket_, buf, "\r\n", asio::use_awaitable);
      std::string header_line;
      std::getline(stream, header_line);
      // Header part ends
      if (header_line.size() <= 2) break;

      // Trim "\r\n"
      std::string_view trimmed{header_line.begin(), header_line.end() - 2};
      const auto delimiter = ": ";
      const auto pos = trimmed.find_first_of(delimiter);
      if (pos == std::string::npos) {
        spdlog::error("[tunnel] parse request header failed, line={}", trimmed);
        throw SocksException{"parse request header failed"};
      }

      std::string key{trimmed.substr({}, pos)}, value{trimmed.substr(pos + 2)};
      entity.headers.emplace(std::move(key), std::move(value));
    }
    co_return entity;
  }

  asio::awaitable<void> LoopOnce() {
    auto len = co_await socket_.async_read_some(asio::buffer(local_buf_),

                                                asio::use_awaitable);
    spdlog::debug("[tunnel] >-- len={}", len);
    co_await socket_.async_write_some(asio::buffer(local_buf_, len),
                                      asio::use_awaitable);
    spdlog::debug("[tunnel] --> len={}", len);
  }

  void Close() {
    if (socket_.is_open()) {
      spdlog::info("[tunnel] socket closed, ep=", socket_.local_endpoint());
      socket_.close();
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
      : acceptor_{ctx_, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), port}} {}
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
        spdlog::error("accept exception, e={}", e.what());
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
