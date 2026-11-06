execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_renderer_hooks.py"
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
set(firefly_filter_source_count 0)
set(firefly_iterator_binding_count 0)
set(firefly_walker_binding_count 0)
set(firefly_custom_event_source_count 0)
set(firefly_custom_binding_source_count 0)
set(firefly_error_event_source_count 0)
set(firefly_error_binding_source_count 0)
set(firefly_message_event_source_count 0)
set(firefly_message_binding_source_count 0)
set(firefly_web_adapted_sources)
foreach(source IN LISTS firefly_web_sources)
    if(source STREQUAL "WebIDL/AbstractOperations.cpp"
       OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/WebIDL/AbstractOperations.cpp")
        math(EXPR firefly_callback_source_count "${firefly_callback_source_count}+1")
        list(APPEND firefly_web_adapted_sources
             "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/CallbackAdapter.cpp")
    elseif(source STREQUAL "HTML/WindowOrWorkerGlobalScope.cpp"
           OR source STREQUAL
              "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/WindowOrWorkerGlobalScope.cpp")
        math(EXPR firefly_timer_source_count "${firefly_timer_source_count}+1")
        list(APPEND firefly_web_adapted_sources
             "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/timers/TimerMapAdapter.cpp")
    elseif(source STREQUAL "DOM/MutationObserver.cpp"
           OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/MutationObserver.cpp")
        math(EXPR firefly_observer_source_count "${firefly_observer_source_count}+1")
        list(
            APPEND firefly_web_adapted_sources
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/observers/ObserverAdapter.cpp")
    elseif(source STREQUAL "DOM/EventTarget.cpp"
           OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/EventTarget.cpp")
        math(EXPR firefly_listener_source_count "${firefly_listener_source_count}+1")
        list(APPEND firefly_web_adapted_sources
             "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/ListenerAdapter.cpp")
    elseif(source STREQUAL "DOM/NodeFilter.cpp"
           OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/NodeFilter.cpp")
        math(EXPR firefly_filter_source_count "${firefly_filter_source_count}+1")
        list(
            APPEND
            firefly_web_adapted_sources
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/traversal/NodeFilterAdapter.cpp"
        )
    elseif(source STREQUAL "Bindings/NodeIterator.cpp"
           OR source STREQUAL "${firefly_web_binary_dir}/Bindings/NodeIterator.cpp")
        math(EXPR firefly_iterator_binding_count "${firefly_iterator_binding_count}+1")
        list(
            APPEND
            firefly_web_adapted_sources
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/traversal/NodeIteratorBindingAdapter.cpp"
        )
    elseif(source STREQUAL "Bindings/TreeWalker.cpp"
           OR source STREQUAL "${firefly_web_binary_dir}/Bindings/TreeWalker.cpp")
        math(EXPR firefly_walker_binding_count "${firefly_walker_binding_count}+1")
        list(
            APPEND
            firefly_web_adapted_sources
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/traversal/TreeWalkerBindingAdapter.cpp"
        )
    elseif(source STREQUAL "DOM/CustomEvent.cpp"
           OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/CustomEvent.cpp")
        math(EXPR firefly_custom_event_source_count "${firefly_custom_event_source_count}+1")
        list(
            APPEND firefly_web_adapted_sources
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/CustomEventAdapter.cpp")
    elseif(source STREQUAL "Bindings/CustomEvent.cpp"
           OR source STREQUAL "${firefly_web_binary_dir}/Bindings/CustomEvent.cpp")
        math(EXPR firefly_custom_binding_source_count "${firefly_custom_binding_source_count}+1")
        list(
            APPEND
            firefly_web_adapted_sources
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/CustomEventBindingAdapter.cpp"
        )
    elseif(source STREQUAL "HTML/ErrorEvent.cpp"
           OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/ErrorEvent.cpp")
        math(EXPR firefly_error_event_source_count "${firefly_error_event_source_count}+1")
        list(APPEND firefly_web_adapted_sources
             "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/ErrorEventAdapter.cpp")
    elseif(source STREQUAL "Bindings/ErrorEvent.cpp"
           OR source STREQUAL "${firefly_web_binary_dir}/Bindings/ErrorEvent.cpp")
        math(EXPR firefly_error_binding_source_count "${firefly_error_binding_source_count}+1")
        list(
            APPEND
            firefly_web_adapted_sources
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/ErrorEventBindingAdapter.cpp"
        )
    elseif(source STREQUAL "HTML/MessageEvent.cpp"
           OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/MessageEvent.cpp")
        math(EXPR firefly_message_event_source_count "${firefly_message_event_source_count}+1")
        list(
            APPEND firefly_web_adapted_sources
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/MessageEventAdapter.cpp"
        )
    elseif(source STREQUAL "Bindings/MessageEvent.cpp"
           OR source STREQUAL "${firefly_web_binary_dir}/Bindings/MessageEvent.cpp")
        math(EXPR firefly_message_binding_source_count "${firefly_message_binding_source_count}+1")
        list(
            APPEND
            firefly_web_adapted_sources
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/MessageEventBindingAdapter.cpp"
        )
    elseif(IS_ABSOLUTE "${source}" OR source MATCHES "^\\$<")
        list(APPEND firefly_web_adapted_sources "${source}")
    elseif(EXISTS "${firefly_web_source_dir}/${source}")
        list(APPEND firefly_web_adapted_sources "${firefly_web_source_dir}/${source}")
    else()
        # Upstream's relative generated sources resolve in its binary directory.
        list(APPEND firefly_web_adapted_sources "${firefly_web_binary_dir}/${source}")
    endif()
endforeach()
if(NOT firefly_iterator_binding_count EQUAL 1
   OR NOT firefly_walker_binding_count EQUAL 1
   OR NOT firefly_filter_source_count EQUAL 1
   OR NOT firefly_callback_source_count EQUAL 1
   OR NOT firefly_timer_source_count EQUAL 1
   OR NOT firefly_observer_source_count EQUAL 1
   OR NOT firefly_listener_source_count EQUAL 1
   OR NOT firefly_custom_event_source_count EQUAL 1
   OR NOT firefly_custom_binding_source_count EQUAL 1
   OR NOT firefly_error_event_source_count EQUAL 1
   OR NOT firefly_error_binding_source_count EQUAL 1
   OR NOT firefly_message_event_source_count EQUAL 1
   OR NOT firefly_message_binding_source_count EQUAL 1)
    message(
        FATAL_ERROR
            "Expected exactly one callback, timer, observer, listener, node filter and native/generated custom/error/message event translation unit; review upstream sources"
    )
endif()
# Owned adapters compile as LibWeb units, so they need our subsystem include root independently of
# the firefly-isolated-world consumer target.
foreach(source IN LISTS firefly_web_adapted_sources)
    if(source MATCHES "^${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/")
        set_property(
            SOURCE "${source}" TARGET_DIRECTORY LibWeb
            APPEND
            PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
    endif()
endforeach()
set_property(TARGET LibWeb PROPERTY SOURCES "${firefly_web_adapted_sources}")
add_dependencies(LibWeb check-firefly-upstream)
add_custom_target(
    check-firefly-callback-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_event_entries.py"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_renderer_hooks.py"
    VERBATIM)
add_dependencies(LibWeb check-firefly-callback-hooks)

# Rename definitions only: preprocessor token renaming also bypasses guards at internal calls in the
# same upstream TU. Retain original algorithms while adapting explicit map, tracing, realm and
# document-scope seams.
set(firefly_event_entries "${CMAKE_BINARY_DIR}/firefly-adapters/EventTarget.inc")
add_custom_command(
    OUTPUT "${firefly_event_entries}"
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/adapt_event_entries.py" --source
        "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/EventTarget.cpp" --output
        "${firefly_event_entries}"
    DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/adapt_event_entries.py"
            "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/EventTarget.cpp"
    VERBATIM)
add_custom_target(firefly-event-entries DEPENDS "${firefly_event_entries}")
add_dependencies(LibWeb firefly-event-entries)
set_property(
    SOURCE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/ListenerAdapter.cpp"
           TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/firefly-adapters")

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
    add_custom_command(
        OUTPUT "${firefly_custom_output}"
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/adapt_custom_event.py" --source
            "${firefly_custom_original}" --output "${firefly_custom_output}" --kind
            "${firefly_custom_kind}"
        DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/adapt_custom_event.py" "${firefly_custom_original}"
        VERBATIM)
    add_custom_target(firefly-custom-event-${kind} DEPENDS "${firefly_custom_output}")
    add_dependencies(LibWeb firefly-custom-event-${kind})
    set_property(
        SOURCE
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/${firefly_custom_adapter}"
            TARGET_DIRECTORY
            LibWeb
        APPEND
        PROPERTY INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/firefly-adapters")
endforeach()
add_custom_target(
    check-firefly-custom-event-hooks
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_custom_event_adapter.py"
        --native "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/CustomEvent.cpp" --binding
        "${firefly_web_binary_dir}/Bindings/CustomEvent.cpp"
    DEPENDS firefly-custom-event-Native firefly-custom-event-Binding
    VERBATIM)
add_dependencies(LibWeb check-firefly-custom-event-hooks)

foreach(kind IN ITEMS Native Binding)
    if(kind STREQUAL "Native")
        set(firefly_error_original "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/ErrorEvent.cpp")
        set(firefly_error_kind native)
        set(firefly_error_adapter ErrorEventAdapter.cpp)
    else()
        set(firefly_error_original "${firefly_web_binary_dir}/Bindings/ErrorEvent.cpp")
        set(firefly_error_kind binding)
        set(firefly_error_adapter ErrorEventBindingAdapter.cpp)
    endif()
    set(firefly_error_output "${CMAKE_BINARY_DIR}/firefly-adapters/ErrorEvent${kind}.inc")
    add_custom_command(
        OUTPUT "${firefly_error_output}"
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/adapt_error_event.py" --source
            "${firefly_error_original}" --output "${firefly_error_output}" --kind
            "${firefly_error_kind}"
        DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/adapt_error_event.py"
                "${FIREFLY_SOURCE_DIR}/tooling/adapt_custom_event.py" "${firefly_error_original}"
        VERBATIM)
    add_custom_target(firefly-error-event-${kind} DEPENDS "${firefly_error_output}")
    add_dependencies(LibWeb firefly-error-event-${kind})
    set_property(
        SOURCE
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/${firefly_error_adapter}"
            TARGET_DIRECTORY
            LibWeb
        APPEND
        PROPERTY INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/firefly-adapters")
endforeach()
add_custom_target(
    check-firefly-error-event-hooks
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_error_event_adapter.py"
        --native "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/ErrorEvent.cpp" --binding
        "${firefly_web_binary_dir}/Bindings/ErrorEvent.cpp"
    DEPENDS firefly-error-event-Native firefly-error-event-Binding
    VERBATIM)
add_dependencies(LibWeb check-firefly-error-event-hooks)

foreach(kind IN ITEMS Native Binding)
    if(kind STREQUAL "Native")
        set(firefly_message_original
            "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/MessageEvent.cpp")
        set(firefly_message_kind native)
        set(firefly_message_adapter MessageEventAdapter.cpp)
    else()
        set(firefly_message_original "${firefly_web_binary_dir}/Bindings/MessageEvent.cpp")
        set(firefly_message_kind binding)
        set(firefly_message_adapter MessageEventBindingAdapter.cpp)
    endif()
    set(firefly_message_output "${CMAKE_BINARY_DIR}/firefly-adapters/MessageEvent${kind}.inc")
    add_custom_command(
        OUTPUT "${firefly_message_output}"
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/adapt_message_event.py" --source
            "${firefly_message_original}" --output "${firefly_message_output}" --kind
            "${firefly_message_kind}"
        DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/adapt_message_event.py"
                "${FIREFLY_SOURCE_DIR}/tooling/adapt_custom_event.py" "${firefly_message_original}"
        VERBATIM)
    add_custom_target(firefly-message-event-${kind} DEPENDS "${firefly_message_output}")
    add_dependencies(LibWeb firefly-message-event-${kind})
    set_property(
        SOURCE
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/events/${firefly_message_adapter}"
            TARGET_DIRECTORY
            LibWeb
        APPEND
        PROPERTY INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/firefly-adapters")
endforeach()

add_custom_target(
    check-firefly-message-event-hooks
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_message_event_adapter.py"
        --native "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/MessageEvent.cpp" --binding
        "${firefly_web_binary_dir}/Bindings/MessageEvent.cpp"
    DEPENDS firefly-message-event-Native firefly-message-event-Binding
    VERBATIM)
add_dependencies(LibWeb check-firefly-message-event-hooks)

set(firefly_filter_entry "${CMAKE_BINARY_DIR}/firefly-adapters/NodeFilter.inc")
add_custom_command(
    OUTPUT "${firefly_filter_entry}"
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/adapt_node_filter.py" --source
        "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/NodeFilter.cpp" --output
        "${firefly_filter_entry}"
    DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/adapt_node_filter.py"
            "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/NodeFilter.cpp"
    VERBATIM)
add_custom_target(firefly-node-filter-entry DEPENDS "${firefly_filter_entry}")
add_custom_target(
    check-firefly-node-filter-hook
    COMMAND "${Python3_EXECUTABLE}"
            "${FIREFLY_SOURCE_DIR}/tooling/tests/test_node_filter_adapter.py"
    DEPENDS firefly-node-filter-entry
    VERBATIM)
add_dependencies(LibWeb check-firefly-node-filter-hook)
set_property(
    SOURCE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/traversal/NodeFilterAdapter.cpp"
           TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/firefly-adapters")

foreach(interface IN ITEMS NodeIterator TreeWalker)
    set(firefly_traversal_original "${firefly_web_binary_dir}/Bindings/${interface}.cpp")
    set(firefly_traversal_output "${CMAKE_BINARY_DIR}/firefly-adapters/${interface}Binding.inc")
    add_custom_command(
        OUTPUT "${firefly_traversal_output}"
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/adapt_traversal_filter.py"
            --source "${firefly_traversal_original}" --output "${firefly_traversal_output}"
            --interface "${interface}"
        DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/adapt_traversal_filter.py"
                "${firefly_traversal_original}"
        VERBATIM)
    add_custom_target(firefly-${interface}-filter-binding DEPENDS "${firefly_traversal_output}")
    add_dependencies(LibWeb firefly-${interface}-filter-binding)
    set_property(
        SOURCE
            "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/callbacks/traversal/${interface}BindingAdapter.cpp"
            TARGET_DIRECTORY
            LibWeb
        APPEND
        PROPERTY INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/firefly-adapters")
endforeach()
add_custom_target(
    check-firefly-traversal-filter-hooks
    COMMAND
        "${Python3_EXECUTABLE}"
        "${FIREFLY_SOURCE_DIR}/tooling/tests/test_traversal_filter_adapter.py" --bindings
        "${firefly_web_binary_dir}/Bindings"
    DEPENDS firefly-NodeIterator-filter-binding firefly-TreeWalker-filter-binding
    VERBATIM)
add_dependencies(LibWeb check-firefly-traversal-filter-hooks)
