#include "observer/network_observer.h"

#include <fmt/ostream.h>

#include "utility/log.h"

namespace socks {

NetworkRelay::NetworkRelay() : pool_{1} {}

NetworkRelay::~NetworkRelay() {}

void NetworkRelay::Start() {}

void NetworkRelay::Connect(size_t idx, asio::ip::tcp::endpoint src,
                           asio::ip::tcp::endpoint dst, std::string host) {
  asio::post(pool_, [this, idx, src, dst, host = std::move(host)]() mutable {
    AsyncConnect(idx, src, dst, std::move(host));
  });
}

void NetworkRelay::Forward(size_t idx, bool outside, std::string s) {
  asio::post(pool_, [this, idx, outside, s = std::move(s)]() mutable {
    AsyncForward(idx, outside, std::move(s));
  });
}

void NetworkRelay::Disconnect(size_t idx) {
  asio::post(pool_, [this, idx] { AsyncDisconnect(idx); });
}

void socks::NetworkRelay::AsyncConnect(size_t idx, asio::ip::tcp::endpoint src,
                                       asio::ip::tcp::endpoint dst,
                                       std::string host) {
  SPDLOG_INFO("[net] remote connected, src={}, dst={}, host={}, idx={}", src,
              dst, host, idx);
  conns_.insert({idx, ConnModel{src, dst, host}});
  for (const auto &observer : observers_) {
    observer->Connect(idx, src, dst, host);
  }
}

void socks::NetworkRelay::AsyncForward(size_t idx, bool outside,
                                       std::string s) {
  SPDLOG_DEBUG("[net] forward packet, outside={}, size={}, idx={}", outside,
               s.size(), idx);
  packets_.emplace_back(idx, outside, s);
  for (const auto &observer : observers_) {
    observer->Forward(idx, outside, s);
  }
}

void socks::NetworkRelay::AsyncDisconnect(size_t idx) {
  SPDLOG_INFO("[net] remote disconnected, idx={}", idx);
  auto it = conns_.find(idx);
  if (it == conns_.end()) {
    return;
  }
  it->second.online = false;
  for (const auto &observer : observers_) {
    observer->Disconnect(idx);
  }
}

}  // namespace socks
