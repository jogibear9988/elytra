set(ipc_page "${CMAKE_BINARY_DIR}/firefly-generated/WebContentPage.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_ipc_event_hook.py"
            --output "${ipc_page}" RESULT_VARIABLE ipc_check)
if(NOT ipc_check EQUAL 0)
    message(FATAL_ERROR "Review owned native IPC event adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_ipc_event_hook.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/WebContentPage.cpp")
get_target_property(view_sources LibWebView SOURCES)
set(ipc_replaced 0)
set(ipc_sources)
foreach(source IN LISTS view_sources)
    if(source STREQUAL "WebContentPage.cpp"
       OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/WebContentPage.cpp")
        list(APPEND ipc_sources "${ipc_page}")
        math(EXPR ipc_replaced "${ipc_replaced}+1")
    else()
        list(APPEND ipc_sources "${source}")
    endif()
endforeach()
if(NOT ipc_replaced EQUAL 1)
    message(FATAL_ERROR "Review LibWebView source layout for native IPC")
endif()
set_property(TARGET LibWebView PROPERTY SOURCES "${ipc_sources}")
target_sources(LibWebView PRIVATE "${CMAKE_CURRENT_LIST_DIR}/TransportHooks.cpp")
target_include_directories(LibWebView PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
add_custom_target(
    check-firefly-ipc-hook
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_ipc_event_hook.py"
            --output "${ipc_page}"
    VERBATIM)
add_dependencies(LibWebView check-firefly-ipc-hook)
