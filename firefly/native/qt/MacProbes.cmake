# These OS probes use AppKit directly; do not build them as Linux placeholders.
add_executable(firefly-clipboard-probe "${CMAKE_CURRENT_LIST_DIR}/../clipboard/Probe.mm"
                                       "${CMAKE_CURRENT_LIST_DIR}/../clipboard/ClipboardMac.mm")
target_link_libraries(firefly-clipboard-probe PRIVATE firefly-clipboard-protocol
                                                      "-framework AppKit")

add_executable(firefly-theme-probe "${CMAKE_CURRENT_LIST_DIR}/../theme/Probe.mm"
                                   "${CMAKE_CURRENT_LIST_DIR}/../theme/ThemeMac.mm")
target_link_libraries(firefly-theme-probe PRIVATE Qt::Core "-framework AppKit")

add_executable(firefly-geometry-probe "${CMAKE_CURRENT_LIST_DIR}/../window/GeometryProbe.cpp"
                                      "${CMAKE_CURRENT_LIST_DIR}/../window/FrameGeometry.cpp")
target_link_libraries(firefly-geometry-probe PRIVATE Qt::Core)

add_executable(firefly-screen-probe "${CMAKE_CURRENT_LIST_DIR}/../screen/Probe.cpp"
                                    "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenMac.mm")
target_link_libraries(firefly-screen-probe PRIVATE Qt::Core "-framework AppKit"
                                                   "-framework CoreGraphics")
