# Main-side file body only: no LibWeb, GUI, VM or Node dependency.
add_library(firefly-protocol-files STATIC "${CMAKE_CURRENT_LIST_DIR}/FileBody.cpp")
target_include_directories(firefly-protocol-files PUBLIC "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-protocol-files PUBLIC LibCore AK)
add_executable(
    firefly-protocol-files-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/FileBodyProbe.cpp"
                                 "${FIREFLY_SOURCE_DIR}/firefly/tests/ipc/SpawnBytePeer.cpp")
target_link_libraries(firefly-protocol-files-probe PRIVATE firefly-protocol-files
                                                           firefly-byte-channel LibMain)
ladybird_check_skia_confinement(firefly-protocol-files-probe)
if(WIN32)
    ladybird_windows_bin(firefly-protocol-files-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-protocol-files
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_protocol_files.py"
            --probe "$<TARGET_FILE:firefly-protocol-files-probe>"
    DEPENDS firefly-protocol-files-probe
    VERBATIM)
