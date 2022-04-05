#pragma once

namespace socks {

class NonCopyable {
 public:
  NonCopyable() = default;
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
};

class Movable {
 public:
  Movable() = default;
  Movable(Movable &&) = default;
  Movable &operator=(Movable &&) = default;
};

}  // namespace socks
