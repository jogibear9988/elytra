# QtCore-only protocol validation is independent of any native clipboard or GUI.
add_library(firefly-clipboard-protocol STATIC "${CMAKE_CURRENT_LIST_DIR}/Protocol.cpp")
target_link_libraries(firefly-clipboard-protocol PUBLIC Qt::Core)
add_executable(firefly-clipboard-protocol-probe "${CMAKE_CURRENT_LIST_DIR}/ProtocolProbe.cpp")
target_link_libraries(firefly-clipboard-protocol-probe PRIVATE firefly-clipboard-protocol)

# Compile this portable Qt seam on macOS as well, but never link it into the AppKit runtime or
# execute its clipboard probe on the user's desktop.
add_library(firefly-x11-clipboard STATIC "${CMAKE_CURRENT_LIST_DIR}/ClipboardX11.cpp")
target_link_libraries(firefly-x11-clipboard PUBLIC firefly-clipboard-protocol Qt::Gui)
add_executable(firefly-x11-clipboard-probe "${CMAKE_CURRENT_LIST_DIR}/X11Probe.cpp")
target_link_libraries(firefly-x11-clipboard-probe PRIVATE firefly-x11-clipboard)
