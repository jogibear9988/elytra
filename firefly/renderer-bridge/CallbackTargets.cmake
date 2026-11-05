execute_process(COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_renderer_hooks.py"
    RESULT_VARIABLE firefly_callback_check)
if(NOT firefly_callback_check EQUAL 0)
    message(FATAL_ERROR "Review the owned WebIDL callback adapter")
endif()
get_target_property(firefly_web_sources LibWeb SOURCES)
get_target_property(firefly_web_source_dir LibWeb SOURCE_DIR)
get_target_property(firefly_web_binary_dir LibWeb BINARY_DIR)
set(firefly_callback_source_count 0)
set(firefly_timer_source_count 0)
set(firefly_observer_source_count 0)
set(firefly_listener_source_count 0)
set(firefly_custom_event_source_count 0)
set(firefly_custom_binding_source_count 0)
set(firefly_web_adapted_sources)
foreach(source IN LISTS firefly_web_sources)
    if(source STREQUAL "WebIDL/AbstractOperations.cpp" OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/WebIDL/AbstractOperations.cpp")
        math(EXPR firefly_callback_source_count "${firefly_callback_source_count}+1")
        list(APPEND firefly_web_adapted_sources "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/CallbackAdapter.cpp")
    elseif(source STREQUAL "HTML/WindowOrWorkerGlobalScope.cpp" OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/WindowOrWorkerGlobalScope.cpp")
        math(EXPR firefly_timer_source_count "${firefly_timer_source_count}+1")
        list(APPEND firefly_web_adapted_sources "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/TimerMapAdapter.cpp")
    elseif(source STREQUAL "DOM/MutationObserver.cpp" OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/MutationObserver.cpp")
        math(EXPR firefly_observer_source_count "${firefly_observer_source_count}+1")
        list(APPEND firefly_web_adapted_sources "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/ObserverAdapter.cpp")
    elseif(source STREQUAL "DOM/EventTarget.cpp" OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/EventTarget.cpp")
        math(EXPR firefly_listener_source_count "${firefly_listener_source_count}+1")
        list(APPEND firefly_web_adapted_sources "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/ListenerAdapter.cpp")
    elseif(source STREQUAL "DOM/CustomEvent.cpp" OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/CustomEvent.cpp")
        math(EXPR firefly_custom_event_source_count "${firefly_custom_event_source_count}+1")
        list(APPEND firefly_web_adapted_sources "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/CustomEventAdapter.cpp")
    elseif(source STREQUAL "Bindings/CustomEvent.cpp" OR source STREQUAL "${firefly_web_binary_dir}/Bindings/CustomEvent.cpp")
        math(EXPR firefly_custom_binding_source_count "${firefly_custom_binding_source_count}+1")
        list(APPEND firefly_web_adapted_sources "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/CustomEventBindingAdapter.cpp")
    elseif(IS_ABSOLUTE "${source}" OR source MATCHES "^\\$<")
        list(APPEND firefly_web_adapted_sources "${source}")
    elseif(EXISTS "${firefly_web_source_dir}/${source}")
        list(APPEND firefly_web_adapted_sources "${firefly_web_source_dir}/${source}")
    else()
        # Upstream's relative generated sources resolve in its binary directory.
        list(APPEND firefly_web_adapted_sources "${firefly_web_binary_dir}/${source}")
    endif()
endforeach()
if(NOT firefly_callback_source_count EQUAL 1 OR NOT firefly_timer_source_count EQUAL 1 OR NOT firefly_observer_source_count EQUAL 1 OR NOT firefly_listener_source_count EQUAL 1 OR NOT firefly_custom_event_source_count EQUAL 1 OR NOT firefly_custom_binding_source_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one callback, timer, observer, listener and native/generated custom event translation unit; review upstream sources")
endif()
set_property(TARGET LibWeb PROPERTY SOURCES "${firefly_web_adapted_sources}")
add_dependencies(LibWeb check-firefly-upstream)
add_custom_target(check-firefly-callback-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/test_event_entries.py"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_renderer_hooks.py" VERBATIM)
add_dependencies(LibWeb check-firefly-callback-hooks)

# Rename definitions only: preprocessor token renaming also bypasses guards at
# internal calls in the same upstream TU. Retain original algorithms while
# adapting explicit map, tracing, realm and document-scope seams.
set(firefly_event_entries "${CMAKE_BINARY_DIR}/firefly-adapters/EventTarget.inc")
add_custom_command(OUTPUT "${firefly_event_entries}"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/adapt_event_entries.py"
        --source "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/EventTarget.cpp"
        --output "${firefly_event_entries}"
    DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/adapt_event_entries.py"
        "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/EventTarget.cpp"
    VERBATIM)
add_custom_target(firefly-event-entries DEPENDS "${firefly_event_entries}")
add_dependencies(LibWeb firefly-event-entries)
set_property(SOURCE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/ListenerAdapter.cpp"
    TARGET_DIRECTORY LibWeb APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/firefly-adapters")

foreach(kind IN ITEMS Native Binding)
    if(kind STREQUAL "Native")
        set(firefly_custom_original "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/CustomEvent.cpp")
        set(firefly_custom_kind native)
        set(firefly_custom_adapter CustomEventAdapter.cpp)
    else()
        set(firefly_custom_original "${firefly_web_binary_dir}/Bindings/CustomEvent.cpp")
        set(firefly_custom_kind binding)
        set(firefly_custom_adapter CustomEventBindingAdapter.cpp)
    endif()
    set(firefly_custom_output "${CMAKE_BINARY_DIR}/firefly-adapters/CustomEvent${kind}.inc")
    add_custom_command(OUTPUT "${firefly_custom_output}"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/adapt_custom_event.py"
            --source "${firefly_custom_original}" --output "${firefly_custom_output}" --kind "${firefly_custom_kind}"
        DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/adapt_custom_event.py" "${firefly_custom_original}" VERBATIM)
    add_custom_target(firefly-custom-event-${kind} DEPENDS "${firefly_custom_output}")
    add_dependencies(LibWeb firefly-custom-event-${kind})
    set_property(SOURCE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/${firefly_custom_adapter}"
        TARGET_DIRECTORY LibWeb APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/firefly-adapters")
endforeach()
add_custom_target(check-firefly-custom-event-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/test_custom_event_adapter.py"
        --native "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/CustomEvent.cpp"
        --binding "${firefly_web_binary_dir}/Bindings/CustomEvent.cpp"
    DEPENDS firefly-custom-event-Native firefly-custom-event-Binding VERBATIM)
add_dependencies(LibWeb check-firefly-custom-event-hooks)
