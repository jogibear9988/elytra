# QtCore-only protocol validation is independent of any native clipboard or GUI.
add_library(firefly-clipboard-protocol STATIC "${CMAKE_CURRENT_LIST_DIR}/Protocol.cpp")
target_link_libraries(firefly-clipboard-protocol PUBLIC Qt::Core)
add_executable(firefly-clipboard-protocol-probe "${CMAKE_CURRENT_LIST_DIR}/ProtocolProbe.cpp")
target_link_libraries(firefly-clipboard-protocol-probe PRIVATE firefly-clipboard-protocol)
