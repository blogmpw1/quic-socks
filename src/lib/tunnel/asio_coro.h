//
// Created by suun on 2022/4/6.
// Copyright (c) 2022 Alibaba. All rights reserved.
//

#ifndef QUIC_SOCKS_TUNNEL_ASIO_CORE_H_
#define QUIC_SOCKS_TUNNEL_ASIO_CORE_H_

#include <asio.hpp>
#include <type_traits>

namespace socks::tunnel {

namespace detail {

template <typename, template <typename...> class>
struct IsSpec : std::false_type {};
template <template <typename...> class Ref, typename... Args>
struct IsSpec<Ref<Args...>, Ref> : std::true_type {};

template <typename Arg, typename... Args>
concept AwaitAbles = IsSpec<Arg, asio::awaitable>::value &&
    std::conjunction_v<std::is_same<Arg, Args>...>;

template <typename Ctx>
concept IsAsioContext = std::is_convertible_v<Ctx &, asio::execution_context &>;
}  // namespace detail

template <detail::IsAsioContext Ctx, detail::AwaitAbles... Args>
auto WaitAll(Ctx &ctx, std::chrono::seconds timeout, Args &&...args)
    -> asio::awaitable<void> {
  //  -> typename std::tuple_element_t<0, std::tuple<Args...>>::value_type {
  asio::steady_timer barrier{ctx, timeout};
  std::atomic_size_t count{sizeof...(Args)};
  (asio::co_spawn(
       ctx,
       [c = std::move(args), &barrier, &count]() mutable -> asio::awaitable<void> {
         co_await std::move(c);
         if (--count == 0) barrier.cancel();
       },
       asio::detached),
   ...);
  co_await barrier.async_wait(asio::use_awaitable);
}

}  // namespace socks::tunnel

#endif  // QUIC_SOCKS_TUNNEL_ASIO_CORE_H_
