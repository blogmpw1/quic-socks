#pragma once
//
// Created by suun 2022/4/6.
//

#include <map>
#include <ostream>
#include <string>
#include <string_view>

#include "utility/result.h"

namespace socks::tunnel {

struct Uri {
  uint16_t port{};
  std::string scheme;
  std::string host;
  std::string path;

  static Uri Parse(std::string_view s);

  friend std::ostream &operator<<(std::ostream &os, const Uri &uri);
};

struct RequestEntity {
  std::string method;
  std::string uri;
  std::string ver;
  std::multimap<std::string, std::string> headers;

  static Result<RequestEntity, SocksException> Parse(std::string_view s);

  friend std::ostream &operator<<(std::ostream &os, const RequestEntity &req);
  RequestEntity() = default;

  [[nodiscard]] std::string Dump() const;
};
}  // namespace socks::tunnel
