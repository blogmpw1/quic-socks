cmake_minimum_required(VERSION 3.21)
project(quic_socks)

set(CMAKE_CXX_STANDARD 23)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
add_subdirectory(src)
add_subdirectory(test)
