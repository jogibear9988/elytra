# Raw-byte transport only: neither JS engine nor renderer code belongs in this target.
add_library(firefly-byte-channel STATIC "${CMAKE_CURRENT_LIST_DIR}/Channel.cpp"
                                        "${CMAKE_CURRENT_LIST_DIR}/Packet.cpp")
target_include_directories(firefly-byte-channel PUBLIC "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-byte-channel PUBLIC LibIPC LibCore AK)
add_executable(
    firefly-byte-channel-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/ipc/ByteChannelProbe.cpp"
                               "${FIREFLY_SOURCE_DIR}/firefly/tests/ipc/SpawnBytePeer.cpp")
target_link_libraries(firefly-byte-channel-probe PRIVATE firefly-byte-channel LibMain)
ladybird_check_skia_confinement(firefly-byte-channel-probe)
if(WIN32)
    ladybird_windows_bin(firefly-byte-channel-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-byte-channel
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_byte_channel.py"
            --probe "$<TARGET_FILE:firefly-byte-channel-probe>"
    DEPENDS firefly-byte-channel-probe
    VERBATIM)
