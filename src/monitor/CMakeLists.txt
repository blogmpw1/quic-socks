set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_PREFIX_PATH "C:/data/qt/qt5.12.12/5.12.12/msvc2017_64")
find_package(Qt5 COMPONENTS Widgets REQUIRED)
add_executable(monitor main.cc monitor.cc monitor.qrc monitor.ui)
target_link_libraries(monitor Qt5::Widgets quic_socks)

add_custom_command(
    TARGET monitor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Qt5::Core>
        $<TARGET_FILE:Qt5::Gui>
        $<TARGET_FILE:Qt5::Widgets>
        $<TARGET_FILE_DIR:monitor>
)