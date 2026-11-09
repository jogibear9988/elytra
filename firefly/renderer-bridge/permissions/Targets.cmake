# Exact source composition keeps all upstream files immutable and drift visible.
include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
set(permission_output "${CMAKE_BINARY_DIR}/firefly-adapters/permissions")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_permission_hooks.py"
            --output-dir "${permission_output}" RESULT_VARIABLE permission_check)
if(NOT permission_check EQUAL 0)
    message(FATAL_ERROR "Review native session permission source adapters")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/prepare_permission_hooks.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/PermissionsAPI/Permissions.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/NotificationsAPI/Notification.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/WebContentPage.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/WebContentClient.cpp")
firefly_replace_source(LibWeb "Libraries/LibWeb/PermissionsAPI/Permissions.cpp"
                       "${permission_output}/Permissions.cpp")
firefly_replace_source(LibWeb "Libraries/LibWeb/NotificationsAPI/Notification.cpp"
                       "${permission_output}/Notification.cpp")
# WebContentPage is composed by prepare_ipc_event_hook.py so both boundaries survive.
firefly_replace_source(LibWebView "Libraries/LibWebView/WebContentClient.cpp"
                       "${permission_output}/WebContentClient.cpp")
target_sources(LibWeb PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Hooks.cpp")
target_sources(LibWebView PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/native/permissions/Hooks.cpp"
                                  "${FIREFLY_SOURCE_DIR}/firefly/native/permissions/Adapter.cpp")
target_include_directories(LibWeb PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
target_include_directories(LibWebView PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
add_custom_target(
    check-firefly-permission-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_permission_hooks.py"
            --output-dir "${permission_output}"
    VERBATIM)
add_dependencies(LibWeb check-firefly-permission-hooks)
add_dependencies(LibWebView check-firefly-permission-hooks)
