add_library(quic_socks STATIC)

file(GLOB_RECURSE SOURCE_FILES *.cc *.h)
target_sources(quic_socks PRIVATE ${SOURCE_FILES})
target_include_directories(quic_socks
  PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

find_package(asio CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(quic_socks PUBLIC asio::asio spdlog::spdlog fmt::fmt)
