//
// Created by suun on 1/4/2022.
//

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <memory>

namespace socks {
class HttpProxy {
 public:
  explicit HttpProxy(const uint16_t port)
      : acceptor_{ctx_, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), port}} {}
  ~HttpProxy() {
    if (thread_ != nullptr) {
      ctx_.stop();
      thread_->join();
    }
  }
  void Start() {
    thread_ = std::make_unique<std::thread>([this] { ThreadRun(); });
  }

  void ThreadRun() {
    asio::co_spawn(
        ctx_, []() noexcept -> asio::awaitable<void> {}, asio::detached);
    ctx_.run();
  }

 private:
  asio::io_context ctx_;
  asio::ip::tcp::acceptor acceptor_;
  std::unique_ptr<std::thread> thread_;
};
}  // namespace socks
