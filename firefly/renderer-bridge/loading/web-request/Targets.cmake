# Private wire code is independently linked into each process. It imports neither LibWeb nor the
# desktop API; no Node/V8 dependency enters the renderer boundary.
add_library(
    firefly-web-request-wire STATIC "${FIREFLY_SOURCE_DIR}/firefly/native/web-request/Packet.cpp"
                                    "${FIREFLY_SOURCE_DIR}/firefly/native/web-request/Headers.cpp")
target_link_libraries(firefly-web-request-wire PUBLIC AK LibHTTP)
set_target_properties(firefly-web-request-wire PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_sources(LibWeb PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Gate.cpp")
set_property(
    SOURCE "${CMAKE_CURRENT_LIST_DIR}/Gate.cpp" TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(LibWeb PRIVATE firefly-web-request-wire)
target_sources(LibWebView PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/native/web-request/Policy.cpp")
target_sources(LibWebView PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/native/web-request/Authority.cpp")

include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
set(web_request_controller "${CMAKE_BINARY_DIR}/firefly-adapters/FetchController.cpp")
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_web_request_controller.py"
        --output "${web_request_controller}"
    RESULT_VARIABLE web_request_controller_check)
if(NOT web_request_controller_check EQUAL 0)
    message(FATAL_ERROR "Review the private Fetch cancellation adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/prepare_web_request_controller.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/Fetch/Infrastructure/FetchController.cpp")
firefly_replace_source(LibWeb "Libraries/LibWeb/Fetch/Infrastructure/FetchController.cpp"
                       "${web_request_controller}")
set_property(
    SOURCE "${web_request_controller}" TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")

add_executable(firefly-web-request-packet-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/web-request-native/PacketProbe.cpp")
target_include_directories(firefly-web-request-packet-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-web-request-packet-probe PRIVATE firefly-web-request-wire)
add_executable(
    firefly-web-request-startup-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/web-request/StartupProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-helper/web-request/Startup.cpp")
target_include_directories(
    firefly-web-request-startup-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly"
                                              "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
target_link_libraries(firefly-web-request-startup-probe PRIVATE LibWeb LibGC AK)
set(web_request_socket "${CMAKE_BINARY_DIR}/firefly-adapters/WebSocket.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_web_request_websocket.py"
            --output "${web_request_socket}" RESULT_VARIABLE web_request_socket_check)
if(NOT web_request_socket_check EQUAL 0)
    message(FATAL_ERROR "Review private WebSocket request-policy containment")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/prepare_web_request_websocket.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/WebSockets/WebSocket.cpp")
firefly_replace_source(LibWeb "Libraries/LibWeb/WebSockets/WebSocket.cpp" "${web_request_socket}")
set_property(
    SOURCE "${web_request_socket}" TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
add_custom_target(
    check-firefly-web-request-native
    COMMAND "$<TARGET_FILE:firefly-web-request-packet-probe>"
    COMMAND "$<TARGET_FILE:firefly-web-request-startup-probe>"
    DEPENDS firefly-web-request-packet-probe firefly-web-request-startup-probe
    VERBATIM)
