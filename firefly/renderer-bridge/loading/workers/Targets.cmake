# Compose worker lifecycle hooks into exact pinned service sources, never upstream files.
include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
set(worker_fetch_adapters "${CMAKE_BINARY_DIR}/firefly-adapters/worker-fetch")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_worker_fetch_hooks.py"
            --upstream "${CMAKE_SOURCE_DIR}" --output "${worker_fetch_adapters}"
    RESULT_VARIABLE worker_fetch_status)
if(NOT worker_fetch_status EQUAL 0)
    message(FATAL_ERROR "Review the pinned worker fetch lifecycle")
endif()
set(worker_originals PageClient.cpp Worker/PageHost.cpp Worker/WorkerThread.cpp
                     Worker/WorkerHost.cpp)
set(worker_composed WorkerPageClient.cpp WorkerPageHost.cpp WorkerThread.cpp WorkerHost.cpp)
foreach(index RANGE 0 3)
    list(GET worker_originals ${index} original)
    list(GET worker_composed ${index} composed)
    firefly_replace_source(webcontentservice "Services/WebContent/${original}"
                           "${worker_fetch_adapters}/${composed}")
    set_property(
        SOURCE "${worker_fetch_adapters}/${composed}" TARGET_DIRECTORY webcontentservice
        APPEND
        PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS "${LADYBIRD_SOURCE_DIR}/Services/WebContent/${original}")
endforeach()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_worker_fetch_hooks.py")
target_sources(LibWeb PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Scope.cpp"
                              "${CMAKE_CURRENT_LIST_DIR}/Fetch.cpp")
set_property(
    SOURCE "${CMAKE_CURRENT_LIST_DIR}/Scope.cpp" "${CMAKE_CURRENT_LIST_DIR}/Fetch.cpp"
           TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly"
             "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")

add_custom_target(
    check-firefly-worker-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_worker_fetch_hooks.py"
            --upstream "${CMAKE_SOURCE_DIR}" --output "${worker_fetch_adapters}"
    VERBATIM)
add_dependencies(webcontentservice check-firefly-worker-hooks)
