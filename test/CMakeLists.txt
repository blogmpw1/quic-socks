add_executable(quic_socks_test)
file(GLOB_RECURSE SOURCE_FILES *.cc)
target_sources(quic_socks_test PRIVATE ${SOURCE_FILES})

include(CPM)
CPMAddPackage("gh:google/googletest#release-1.11.0")

target_link_libraries(quic_socks_test PRIVATE gmock quic_socks)
