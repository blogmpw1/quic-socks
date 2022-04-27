#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "utility/log.h"

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  socks::InitAsyncLogger();
  return RUN_ALL_TESTS();
}