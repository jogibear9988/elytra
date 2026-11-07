# Main-side file bodies and response metadata: no LibWeb, GUI, VM or Node dependency.
add_library(firefly-protocol-file-codec STATIC "${CMAKE_CURRENT_LIST_DIR}/ResponseHead.cpp"
                                               "${CMAKE_CURRENT_LIST_DIR}/FilePacket.cpp")
target_include_directories(firefly-protocol-file-codec PUBLIC "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-protocol-file-codec PUBLIC LibCore LibHTTP AK)
add_library(firefly-protocol-files STATIC "${CMAKE_CURRENT_LIST_DIR}/FileBody.cpp"
                                          "${CMAKE_CURRENT_LIST_DIR}/FileRequests.cpp")
target_link_libraries(firefly-protocol-files PUBLIC firefly-protocol-file-codec)
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
