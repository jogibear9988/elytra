# PendingResponse intentionally has no exported upstream C++ API. Keep our owning adapter inside
# LibWeb; export only our owned native C++ boundary.
set(protocol_fetch_source "${CMAKE_CURRENT_LIST_DIR}/Response.cpp"
                          "${CMAKE_CURRENT_LIST_DIR}/Dispatch.cpp")
target_sources(LibWeb PRIVATE ${protocol_fetch_source})
set_property(
    SOURCE ${protocol_fetch_source} TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly"
             "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
target_link_libraries(LibWeb PRIVATE "$<LINK_ONLY:firefly-protocol-file-codec>")
add_library(firefly-protocol-fetch INTERFACE)
target_include_directories(
    firefly-protocol-fetch INTERFACE "${FIREFLY_SOURCE_DIR}/firefly"
                                     "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
target_link_libraries(firefly-protocol-fetch INTERFACE LibWeb LibJS LibGC LibURL
                                                       firefly-protocol-file-codec)
add_library(firefly-protocol-client STATIC "${CMAKE_CURRENT_LIST_DIR}/Client.cpp")
target_link_libraries(firefly-protocol-client PUBLIC firefly-protocol-fetch firefly-byte-channel)
add_executable(
    firefly-protocol-fetch-channel-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/FetchChannelProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/ipc/SpawnBytePeer.cpp")
target_link_libraries(firefly-protocol-fetch-channel-probe
                      PRIVATE firefly-protocol-client firefly-protocol-file-server LibMain)
ladybird_check_skia_confinement(firefly-protocol-fetch-channel-probe)
if(WIN32)
    target_link_libraries(firefly-protocol-fetch-channel-probe PRIVATE shell32)
    ladybird_windows_bin(firefly-protocol-fetch-channel-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-protocol-fetch-channel
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_protocol_fetch_channel.py"
        --probe "$<TARGET_FILE:firefly-protocol-fetch-channel-probe>"
    DEPENDS firefly-protocol-fetch-channel-probe
    VERBATIM)
add_executable(
    firefly-protocol-fetch-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/FetchResponseProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/FetchDispatchProbe.cpp")
target_link_libraries(firefly-protocol-fetch-probe PRIVATE firefly-protocol-fetch
                                                           firefly-protocol-files LibMain)
ladybird_check_skia_confinement(firefly-protocol-fetch-probe)
if(WIN32)
    target_link_libraries(firefly-protocol-fetch-probe PRIVATE shell32)
    ladybird_windows_bin(firefly-protocol-fetch-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-protocol-fetch
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_protocol_fetch.py"
            --probe "$<TARGET_FILE:firefly-protocol-fetch-probe>"
    DEPENDS firefly-protocol-fetch-probe
    VERBATIM)
add_dependencies(check-firefly-protocol-fetch check-firefly-protocol-fetch-channel)
add_dependencies(check-firefly-protocol-fetch check-firefly-protocol-routing)
