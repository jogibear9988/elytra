# In-process Hub keeps canonical document/permission authority in the owned UI process.
if(NOT TARGET hubservice)
    message(FATAL_ERROR "Embedded Firefly requires the pinned upstream hubservice target")
endif()
include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
set(hub_lifecycle_output "${CMAKE_BINARY_DIR}/firefly-adapters/hub/Application.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_hub_lifecycle.py"
            --output "${hub_lifecycle_output}" RESULT_VARIABLE hub_lifecycle_result)
if(NOT hub_lifecycle_result EQUAL 0)
    message(FATAL_ERROR "Review embedded Hub startup source composition")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_hub_lifecycle.py"
             "${LADYBIRD_SOURCE_DIR}/Services/Hub/Application.cpp")
firefly_replace_source(hubservice "Services/Hub/Application.cpp" "${hub_lifecycle_output}")
target_sources(hubservice PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Lifecycle.cpp")
target_include_directories(hubservice PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
add_custom_target(
    check-firefly-hub-lifecycle
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_hub_lifecycle.py"
            --output "${hub_lifecycle_output}"
    VERBATIM)
add_dependencies(hubservice check-firefly-hub-lifecycle)

# IPC may retain ConnectionFromUI until the thread queue dies. Its Application callbacks must be
# explicitly retired first without changing its class layout.
set(hub_connection_output "${CMAKE_BINARY_DIR}/firefly-adapters/hub/ConnectionFromUI.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_hub_connection.py"
            --output "${hub_connection_output}" RESULT_VARIABLE hub_connection_result)
if(NOT hub_connection_result EQUAL 0)
    message(FATAL_ERROR "Review embedded Hub endpoint lifecycle composition")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_hub_connection.py"
             "${LADYBIRD_SOURCE_DIR}/Services/Hub/ConnectionFromUI.cpp")
firefly_replace_source(hubservice "Services/Hub/ConnectionFromUI.cpp" "${hub_connection_output}")
add_custom_target(
    check-firefly-hub-connection
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_hub_connection.py"
            --output "${hub_connection_output}"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_hub_connection.py"
    VERBATIM)
add_dependencies(hubservice check-firefly-hub-connection)
