#include "tunnel/http_proxy.h"
#include "utility/log.h"

int main(int argc, char **argv) {
  socks::InitAsyncLogger();

  const auto proxy = socks::tunnel::HttpProxy::Create(8999);
  proxy->Start();

  std::cin.get();
  return 0;
}
