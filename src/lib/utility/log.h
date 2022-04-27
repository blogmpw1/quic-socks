//
// Created by suun on 2022/4/8.
//

#ifndef QUIC_SOCKS_UTILITY_LOG_H_
#define QUIC_SOCKS_UTILITY_LOG_H_

#define SPDLOG_ACTIVE_LEVEL 0
#include <spdlog/spdlog.h>

#include <iostream>

namespace socks {

void InitAsyncLogger();

}  // namespace socks

#endif  // QUIC_SOCKS_UTILITY_LOG_H_
