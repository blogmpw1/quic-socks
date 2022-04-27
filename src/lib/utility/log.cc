#include "utility/log.h"

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace socks {
void InitAsyncLogger() {
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  spdlog::init_thread_pool(1024, 1);
  auto async_logger = std::make_shared<spdlog::async_logger>(
      "", std::move(stdout_sink), spdlog::thread_pool(),
      spdlog::async_overflow_policy::block);
  spdlog::set_default_logger(std::move(async_logger));
  spdlog::set_level(spdlog::level::debug);
}
}  // namespace socks