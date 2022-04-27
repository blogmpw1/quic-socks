# add_executable(quic_socks_test)
# file(GLOB_RECURSE SOURCE_FILES *.cc)
# target_sources(quic_socks_test PRIVATE ${SOURCE_FILES})

# find_package(GTest CONFIG REQUIRED)
# target_link_libraries(quic_socks_test PRIVATE GTest::gmock quic_socks)

add_executable(http_proxy_example http_proxy_example.cc)
target_link_libraries(http_proxy_example PRIVATE quic_socks)
