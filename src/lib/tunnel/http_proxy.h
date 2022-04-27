#pragma once

#include <memory>

#include "observer/network_observer.h"
#include "utility/ctor.h"

namespace socks::tunnel {

class HttpProxy : Movable, NonCopyable {
 public:
  static std::shared_ptr<HttpProxy> Create(uint16_t port);

  virtual ~HttpProxy() = default;
  virtual void Start() = 0;
  virtual void Register(NetworkObserver *observer) = 0;
};

}  // namespace socks::tunnel
