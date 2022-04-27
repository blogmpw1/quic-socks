//
// Created by suun on 1/4/2022.
//

#include "tunnel/http_proxy.h"

#include <fmt/ostream.h>

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/buffer.hpp>
#include <memory>
#include <regex>
#include <variant>

#include "channel/quic_channel.h"
#include "entities.h"
#include "observer/network_observer.h"
#include "tunnel/asio_helper.h"
#include "utility/log.h"
#include "utility/result.h"

namespace socks::tunnel {

class Session {
 public:
  Session(size_t idx, asio::io_context &ctx, NetworkObserver *observer,
          asio::ip::tcp::socket socket)
      : idx_{idx},
        ctx_{ctx},
        observer_{observer},
        socket_{std::move(socket)},
        remote_{ctx} {}

  void Start() noexcept {
    co_spawn(
        ctx_, [this] { return this->AsyncStart(); }, asio::detached);
  }

  asio::awaitable<void> AsyncStart() {
    RequestEntity entity;
    try {
      auto &&[entity, remain] = co_await ParseRequest();
      const auto uri = Uri::Parse(entity.uri);
      co_await ConnectRemote(uri);

      if (entity.method == "CONNECT") {
        const std::string response = {
            "HTTP/1.1 200 Connection Established\r\n\r\n"};
        co_await socket_.async_write_some(asio::buffer(response),
                                          asio::use_awaitable);
      } else {
        const std::string request = entity.Dump();
        SPDLOG_DEBUG("[tunnel] request remote, data={}, idx={}", request, idx_);
        co_await remote_.async_write_some(asio::buffer(request),
                                          asio::use_awaitable);
      }

      co_await WaitAll(
          ctx_, std::chrono::hours{24},
          [this, &remain]() -> asio::awaitable<void> {
            auto remain_size = remain.size();
            if (remain_size > 0) {
              co_await remote_.async_write_some(asio::buffer(remain),
                                                asio::use_awaitable);
              observer_->Forward(idx_, true, remain);
            }

            co_await ForwardTo(true);
          }(),
          ForwardTo(false));
    } catch (std::runtime_error &e) {
      CloseSocket();
    }
  }

  asio::awaitable<void> ForwardTo(bool outside) {
    std::array<char, 8196> buf{};
    size_t len{0};
    auto &from = outside ? socket_ : remote_;
    auto &to = outside ? remote_ : socket_;
    while (true) {
      try {
        len = co_await from.async_read_some(asio::buffer(buf),
                                            asio::use_awaitable);
      } catch (asio::system_error &e) {
        CloseSocket(&from);
        co_return;
      }

      observer_->Forward(idx_, outside,
                         std::string{buf.begin(), buf.begin() + len});

      try {
        len = co_await to.async_write_some(asio::buffer(buf, len),
                                           asio::use_awaitable);
      } catch (asio::system_error &e) {
        CloseSocket(&to);
        co_return;
      }
    }
  }

  asio::awaitable<void> ConnectRemote(const Uri &uri) {
    asio::error_code err;
    auto address = asio::ip::make_address(uri.host, err);
    if (err) {
      asio::ip::tcp::resolver resolver{ctx_};
      auto &&resolve_res = co_await resolver.async_resolve(
          uri.host, std::to_string(uri.port), asio::use_awaitable);
      if (resolve_res.empty()) {
        throw SocksException(
            fmt::format("[tunnel] resolve dns failed, host={}", uri.host));
      }

      address = resolve_res->endpoint().address();
    }

    const auto ep = asio::ip::tcp::endpoint{address, uri.port};
    co_await remote_.async_connect(ep, asio::use_awaitable);
    observer_->Connect(idx_, socket_.remote_endpoint(),
                       remote_.remote_endpoint(), uri.host);
  }

  asio::awaitable<std::pair<RequestEntity, std::string>> ParseRequest() {
    asio::streambuf buf;
    while (true) {
      auto len = co_await socket_.async_read_some(buf.prepare(1024),
                                                  asio::use_awaitable);
      buf.commit(len);
      if (len == 0) continue;

      const std::string kRequestEnd = "\r\n\r\n";
      const auto const_buf = buf.data();
      const auto buf_begin = static_cast<const char *>(const_buf.data());
      const auto buf_end =
          static_cast<const char *>(const_buf.data()) + const_buf.size();
      if (const auto pos =
              std::search(buf_begin, buf_end, std::begin(kRequestEnd),
                          std::end(kRequestEnd));
          pos != buf_end) {
        const auto raw_len =
            static_cast<size_t>(pos - buf_begin) + kRequestEnd.size();
        buf.consume(raw_len);
        auto res = RequestEntity::Parse(std::string_view{buf_begin, raw_len});
        if (!res) {
          throw std::move(res.Error());
        }

        co_return std::make_pair(res.Value(),
                                 std::string{buf_begin + raw_len, buf_end});
      }
    }
  }

  void CloseSocket(asio::ip::tcp::socket *socket = nullptr) {
    if (socket == nullptr) {
      socket_.close();
      remote_.close();
    } else {
      socket->close();
    }

    if (socket_.is_open() || remote_.is_open()) {
      return;
    }
    observer_->Disconnect(idx_);
  }

 private:
  size_t idx_;
  asio::io_context &ctx_;
  NetworkObserver *observer_;
  asio::ip::tcp::socket socket_;
  asio::ip::tcp::socket remote_;
};

class HttpProxyImpl final : public HttpProxy {
 public:
  explicit HttpProxyImpl(const uint16_t port)
      : ctx_{ASIO_CONCURRENCY_HINT_UNSAFE_IO},
        acceptor_{ctx_, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), port}} {}
  ~HttpProxyImpl() override {
    ctx_.stop();
    for (auto &&t : threads_) {
      t.join();
    }
  }
  void Start() override {
    co_spawn(
        ctx_, [this] { return this->Accept(); }, asio::detached);
    for (int i = 0; i < 8; ++i) {
      threads_.emplace_back([this] { ctx_.run(); });
    }
    relay_.Start();
  }
  void Register(NetworkObserver *observer) override {
    relay_.Register(std::move(observer));
  }

 private:
  asio::awaitable<void> Accept() {
    size_t idx{0};
    std::atomic_size_t cnt{0};
    while (true) {
      try {
        auto &&socket = co_await acceptor_.async_accept(asio::use_awaitable);
        auto session =
            std::make_shared<Session>(idx++, ctx_, &relay_, std::move(socket));
        co_spawn(
            ctx_,
            [session, &cnt]() -> asio::awaitable<void> {
              ++cnt;
              co_await session->AsyncStart();
              --cnt;
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
  std::vector<std::thread> threads_;
  NetworkRelay relay_;
};

std::shared_ptr<HttpProxy> HttpProxy::Create(uint16_t port) {
  return std::make_shared<HttpProxyImpl>(port);
}

}  // namespace socks::tunnel
