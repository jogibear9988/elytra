# Main-side file bodies and response metadata: no LibWeb, GUI, VM or Node dependency.
add_library(
    firefly-protocol-file-codec STATIC
    "${CMAKE_CURRENT_LIST_DIR}/ResponseHead.cpp" "${CMAKE_CURRENT_LIST_DIR}/StreamResponseHead.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/FilePacket.cpp" "${CMAKE_CURRENT_LIST_DIR}/RequestPacket.cpp")
target_include_directories(firefly-protocol-file-codec PUBLIC "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-protocol-file-codec PUBLIC LibCore LibHTTP AK)
add_library(firefly-protocol-files STATIC "${CMAKE_CURRENT_LIST_DIR}/FileBody.cpp"
                                          "${CMAKE_CURRENT_LIST_DIR}/FileRequests.cpp")
target_link_libraries(firefly-protocol-files PUBLIC firefly-protocol-file-codec)
add_library(firefly-protocol-file-server STATIC "${CMAKE_CURRENT_LIST_DIR}/Server.cpp")
target_link_libraries(firefly-protocol-file-server PUBLIC firefly-protocol-files
                                                          firefly-byte-channel)
add_executable(
    firefly-protocol-server-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/ServerProbe.cpp"
                                  "${FIREFLY_SOURCE_DIR}/firefly/tests/ipc/SpawnBytePeer.cpp")
target_link_libraries(firefly-protocol-server-probe PRIVATE firefly-protocol-file-server LibMain)
ladybird_check_skia_confinement(firefly-protocol-server-probe)
if(WIN32)
    target_link_libraries(firefly-protocol-server-probe PRIVATE shell32)
    ladybird_windows_bin(firefly-protocol-server-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-protocol-server
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_protocol_server.py"
            --probe "$<TARGET_FILE:firefly-protocol-server-probe>"
    DEPENDS firefly-protocol-server-probe
    VERBATIM)
add_executable(
    firefly-protocol-files-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/FileBodyProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/ipc/SpawnBytePeer.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/FileRequestsProbe.cpp")
target_link_libraries(firefly-protocol-files-probe
                      PRIVATE firefly-protocol-files firefly-byte-channel LibMain LibFileSystem)
ladybird_check_skia_confinement(firefly-protocol-files-probe)
if(WIN32)
    target_link_libraries(firefly-protocol-files-probe PRIVATE shell32)
    ladybird_windows_bin(firefly-protocol-files-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-protocol-files
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_protocol_files.py"
            --probe "$<TARGET_FILE:firefly-protocol-files-probe>"
    DEPENDS firefly-protocol-files-probe
    VERBATIM)
# Existing release/Linux/Windows gates must also exercise the request direction and the main-side
# resource owner, not only preconstructed response frames.
add_dependencies(check-firefly-protocol-files check-firefly-protocol-server)

# Additive streaming metadata policy; this gate does not install protocol.handle.
add_executable(firefly-protocol-stream-head-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/StreamResponseHeadProbe.cpp")
target_link_libraries(firefly-protocol-stream-head-probe PRIVATE firefly-protocol-file-codec
                                                                 LibMain)
ladybird_check_skia_confinement(firefly-protocol-stream-head-probe)
if(WIN32)
    ladybird_windows_bin(firefly-protocol-stream-head-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-protocol-stream-head
    COMMAND "$<TARGET_FILE:firefly-protocol-stream-head-probe>"
    DEPENDS firefly-protocol-stream-head-probe
    VERBATIM)
add_dependencies(check-firefly-protocol-files check-firefly-protocol-stream-head)

# Genuine asynchronous application producers over the existing authenticated native byte transport.
add_executable(
    firefly-protocol-stream-server-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/StreamServerProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/ipc/SpawnBytePeer.cpp")
target_link_libraries(firefly-protocol-stream-server-probe PRIVATE firefly-protocol-file-server
                                                                   LibMain)
ladybird_check_skia_confinement(firefly-protocol-stream-server-probe)
if(WIN32)
    target_link_libraries(firefly-protocol-stream-server-probe PRIVATE shell32)
    ladybird_windows_bin(firefly-protocol-stream-server-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-protocol-stream-server
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_protocol_stream_server.py"
        --probe "$<TARGET_FILE:firefly-protocol-stream-server-probe>"
    DEPENDS firefly-protocol-stream-server-probe
    VERBATIM)
add_dependencies(check-firefly-protocol-files check-firefly-protocol-stream-server)
