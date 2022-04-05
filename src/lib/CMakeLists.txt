add_library(quic_socks STATIC)

file(GLOB_RECURSE SOURCE_FILES *.cc)
target_sources(quic_socks PRIVATE ${SOURCE_FILES})
target_include_directories(quic_socks
  PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

find_package(Asio REQUIRED)
CPMAddPackage(
  NAME fmt
  GIT_TAG 8.1.1
  GITHUB_REPOSITORY fmtlib/fmt
  VERSION 8.1.1)
CPMAddPackage("gh:gabime/spdlog@1.9.2")
target_link_libraries(quic_socks PUBLIC Asio::Asio spdlog::spdlog fmt::fmt)
