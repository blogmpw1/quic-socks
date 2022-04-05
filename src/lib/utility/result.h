// ReSharper disable CppNonExplicitConvertingConstructor
#pragma once

#include <variant>

namespace socks {

class ReplError {
 public:
  explicit ReplError(std::string &&reason) : reason_(std::move(reason)) {}

  [[nodiscard]] const std::string &reason() const { return reason_; }

 private:
  std::string reason_;
};

class SocksException final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

template <typename T, typename E>
class Result {
 public:
  Result(T &&value) : data_{std::move(value)} {}
  Result(E &&error) : data_{std::move(error)} {}

  const T &Value() const { return std::get<T>(data_); }
  const E &Error() const { return std::get<E>(data_); }
  T &Value() { return std::get<T>(data_); }
  E &Error() { return std::get<E>(data_); }

  // ReSharper disable once CppNonExplicitConversionOperator
  operator bool() const {
    const T *value = std::get_if<T>(&data_);
    return value != nullptr;
  }

 private:
  std::variant<T, E> data_;
};
}  // namespace socks