# Compile native OS providers independently of the still-gated full desktop runtime.
include_guard(GLOBAL)
if(NOT WIN32)
    message(FATAL_ERROR "Windows native providers require a Windows toolchain")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/RuntimePaths.cmake")
ladybird_windows_bin(firefly-runtime-paths-probe CONSOLE)
add_library(
    firefly-windows-desktop-services STATIC
    "${CMAKE_CURRENT_LIST_DIR}/Windows.cpp" "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenWindows.cpp")
target_link_libraries(firefly-windows-desktop-services PUBLIC Qt::Gui LibJS LibGC AK
                                                              firefly-screen-events user32)
add_library(firefly-windows-window-geometry STATIC
            "${CMAKE_CURRENT_LIST_DIR}/../window/GeometryWindows.cpp")
target_link_libraries(firefly-windows-window-geometry PUBLIC Qt::Widgets)
add_executable(firefly-windows-desktop-provider-probe "${CMAKE_CURRENT_LIST_DIR}/WindowsProbe.cpp")
target_link_libraries(firefly-windows-desktop-provider-probe
                      PRIVATE firefly-windows-desktop-services firefly-locale)
ladybird_windows_bin(firefly-windows-desktop-provider-probe CONSOLE)
add_custom_target(
    check-firefly-windows-desktop-provider
    COMMAND "$<TARGET_FILE:firefly-windows-desktop-provider-probe>"
    DEPENDS firefly-windows-desktop-provider-probe
    VERBATIM)
