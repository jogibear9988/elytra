# Share the native title-bar provider across runtime chrome and hidden probes.
add_library(firefly-native-title-bar STATIC)
target_link_libraries(firefly-native-title-bar PUBLIC Qt::Widgets)
if(APPLE)
    target_sources(firefly-native-title-bar PRIVATE "${CMAKE_CURRENT_LIST_DIR}/TitleBarMac.mm")
    target_compile_options(firefly-native-title-bar PRIVATE -fobjc-arc)
    target_link_libraries(firefly-native-title-bar PRIVATE "-framework AppKit")
else()
    target_sources(firefly-native-title-bar
                   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/TitleBarUnsupported.cpp")
endif()

# The real Linux geometry adapter can be compiled on macOS without replacing AppKit centering. Only
# isolated hosted X11 acceptance is allowed to initialize its GUI probe.
add_executable(firefly-native-geometry-probe "${CMAKE_CURRENT_LIST_DIR}/GeometryProbe.cpp"
                                             "${CMAKE_CURRENT_LIST_DIR}/FrameGeometry.cpp")
target_link_libraries(firefly-native-geometry-probe PRIVATE Qt::Core)
add_library(
    firefly-linux-window-geometry STATIC
    "${CMAKE_CURRENT_LIST_DIR}/Geometry.cpp" "${CMAKE_CURRENT_LIST_DIR}/FrameGeometry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/GeometryLinux.cpp")
target_link_libraries(firefly-linux-window-geometry PUBLIC Qt::Widgets)
add_executable(firefly-x11-geometry-probe "${CMAKE_CURRENT_LIST_DIR}/X11GeometryProbe.cpp")
target_link_libraries(firefly-x11-geometry-probe PRIVATE firefly-linux-window-geometry)
