#pragma once

#include <coroutine>

namespace socks {

template <typename T>
class Task {
  bool await_ready() { return false; }
  void await_suspend(std::coroutine_handle<> handle) { handle.resume(); }
  T await_resume() { return T{}; }
};

}  // namespace socks