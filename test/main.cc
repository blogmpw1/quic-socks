#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <spdlog/spdlog.h>

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  spdlog::set_level(spdlog::level::trace);
  return RUN_ALL_TESTS();
}