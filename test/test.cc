#include <gtest/gtest.h>

#include <thread>

#include "tunnel/http_proxy.h"

TEST(HttpProxyTest, Run) {
  const auto proxy = socks::tunnel::HttpProxy::Create(8999);
  proxy->Start();
  std::this_thread::sleep_for(std::chrono::minutes{60});
}
