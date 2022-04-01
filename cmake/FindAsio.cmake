if (Asio_FOUND)
    return()
endif ()

include(CPM)
CPMAddPackage(
        NAME asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio
        GIT_TAG asio-1-22-1
        VERSION 1.22.1
        DOWNLOAD_ONLY
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Asio
        FOUND_VAR Asio_FOUND
        REQUIRED_VARS asio_ADDED)

add_library(Asio::Asio INTERFACE IMPORTED)
target_include_directories(Asio::Asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(Asio::Asio INTERFACE ASIO_STANDALONE)
target_compile_features(Asio::Asio INTERFACE cxx_std_20)
if(WIN32 AND CMAKE_SYSTEM_VERSION)
    set(ver ${CMAKE_SYSTEM_VERSION})
    string(REPLACE "." "" ver ${ver})
    string(REGEX REPLACE "([0-9])" "0\\1" ver ${ver})
    set(version "0x${ver}")
    target_compile_definitions(Asio::Asio INTERFACE _WIN32_WINNT=${version})
endif()
