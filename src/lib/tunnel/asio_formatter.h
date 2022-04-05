#pragma once

#include <fmt/format.h>

#include <asio.hpp>

template <>
struct fmt::formatter<asio::ip::tcp::endpoint> {
  static auto parse(const fmt::format_parse_context &ctx)
      -> decltype(ctx.begin()) {
    return ctx.begin();
  }
  template <typename FormatCtx>
  auto format(const asio::ip::tcp::endpoint &ep, FormatCtx &ctx)
      -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}:{}", ep.address().to_string(),
                          ep.port());
  }
};