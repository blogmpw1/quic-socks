add_library(quic-socks STATIC)

file(GLOB_RECURSE SOURCE_FILES *.cc)

find_package(Asio REQUIRED)
CPMAddPackage(
  NAME fmt
  GIT_TAG 8.1.1
  GITHUB_REPOSITORY fmtlib/fmt
  VERSION 8.1.1)

target_sources(quic-socks PRIVATE ${SOURCE_FILES})
target_link_libraries(quic-socks PRIVATE Asio::Asio)
