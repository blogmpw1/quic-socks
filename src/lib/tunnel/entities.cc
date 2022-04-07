//
// Created by suun on 2022/4/6.
//

#include "tunnel/entities.h"

#include <fmt/format.h>

#include <regex>

#include "utility/result.h"

namespace socks::tunnel {

Uri Uri::Parse(std::string_view s) {
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
std::ostream &operator<<(std::ostream &os, const Uri &uri) {
  return os << fmt::format("{}:{}{}", uri.host, uri.port, uri.path);
}

std::ostream &operator<<(std::ostream &os, const RequestEntity &req) {
  return os << fmt::format("{} {} {}", req.method, req.uri, req.ver);
}
std::string RequestEntity::Dump() const {
  fmt::memory_buffer buf;
  const auto path = Uri::Parse(uri).path;
  fmt::format_to(std::back_inserter(buf), "{} {} {}\r\n", method, path, ver);
  for (const auto &[k, v] : headers) {
    fmt::format_to(std::back_inserter(buf), "{}: {}\r\n", k, v);
  }
  fmt::format_to(std::back_inserter(buf), "\r\n");
  return fmt::to_string(buf);
}
}  // namespace socks::tunnel