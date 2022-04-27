#pragma once

#include <asio/ip/tcp.hpp>
#include <asio/thread_pool.hpp>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace socks {

class NetworkObserver {
 public:
  virtual void Connect(size_t idx, asio::ip::tcp::endpoint src,
                       asio::ip::tcp::endpoint dst, std::string_view host) = 0;
  virtual void Forward(size_t idx, bool outside, std::string_view data) = 0;
  virtual void Disconnect(size_t idx) = 0;
};

class NetworkRelay : public NetworkObserver {
 public:
  NetworkRelay();
  ~NetworkRelay();

  void Start();
  void Register(NetworkObserver *observer) {
    observers_.emplace_back(std::move(observer));
  }
  void Connect(size_t idx, asio::ip::tcp::endpoint src,
               asio::ip::tcp::endpoint dst, std::string_view host) override;
  void Forward(size_t idx, bool outside, std::string_view s) override;
  void Disconnect(size_t idx) override;

 private:
  struct ConnModel {
    bool online{true};
    asio::ip::tcp::endpoint src;
    asio::ip::tcp::endpoint dst;
    std::string host;

    ConnModel(asio::ip::tcp::endpoint src, asio::ip::tcp::endpoint dst,
              std::string host)
        : src{src}, dst{dst}, host{std::move(host)} {}
  };
  struct PacketModel {
    size_t idx;
    bool outside;
    std::chrono::time_point<std::chrono::steady_clock> time;
    std::string data;

    PacketModel(size_t idx, bool outside, std::string data)
        : idx{idx},
          outside{outside},
          time{std::chrono::steady_clock::now()},
          data{std::move(data)} {}
  };

  void AsyncConnect(size_t idx, asio::ip::tcp::endpoint src,
                    asio::ip::tcp::endpoint dst, std::string host);
  void AsyncForward(size_t idx, bool outside, std::string s);
  void AsyncDisconnect(size_t idx);

  asio::thread_pool pool_;
  std::unordered_map<size_t, ConnModel> conns_;
  std::vector<PacketModel> packets_;
  std::vector<NetworkObserver *> observers_;
};

}  // namespace socks
