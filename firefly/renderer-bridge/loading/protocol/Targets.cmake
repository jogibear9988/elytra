# PendingResponse intentionally has no exported upstream C++ API. Keep our owning adapter inside
# LibWeb; export only our owned native C++ boundary.
set(protocol_fetch_source "${CMAKE_CURRENT_LIST_DIR}/Response.cpp")
target_sources(LibWeb PRIVATE "${protocol_fetch_source}")
set_property(
    SOURCE "${protocol_fetch_source}" TARGET_DIRECTORY LibWeb
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
add_executable(firefly-protocol-fetch-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/FetchResponseProbe.cpp")
target_link_libraries(firefly-protocol-fetch-probe PRIVATE firefly-protocol-fetch
                                                           firefly-protocol-files LibMain)
ladybird_check_skia_confinement(firefly-protocol-fetch-probe)
if(WIN32)
    ladybird_windows_bin(firefly-protocol-fetch-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-protocol-fetch
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_protocol_fetch.py"
            --probe "$<TARGET_FILE:firefly-protocol-fetch-probe>"
    DEPENDS firefly-protocol-fetch-probe
    VERBATIM)
