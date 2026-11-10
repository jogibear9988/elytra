# RemoteView's embedded composition removes a browser-store base class. Every frontend consumer must
# therefore select the same header before upstream paths. BEFORE PUBLIC on the library alone cannot
# order a consumer's other includes.
include_guard(GLOBAL)

function(firefly_collect_header_targets directory output)
    get_property(
        targets
        DIRECTORY "${directory}"
        PROPERTY BUILDSYSTEM_TARGETS)
    get_property(
        children
        DIRECTORY "${directory}"
        PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        firefly_collect_header_targets("${child}" nested)
        list(APPEND targets ${nested})
    endforeach()
    set(${output}
        "${targets}"
        PARENT_SCOPE)
endfunction()

function(firefly_apply_hub_header_policy)
    get_property(overlay GLOBAL PROPERTY FIREFLY_HUB_FRONTEND_HEADERS)
    firefly_collect_header_targets("${CMAKE_SOURCE_DIR}" targets)
    # Injected framework targets have the SDK root as SOURCE_DIR too. Derive membership from real
    # dependencies, never from that misleading directory.
    set(consumers firefly-qt-view)
    set(changed TRUE)
    while(changed)
        set(changed FALSE)
        foreach(target IN LISTS targets)
            if(target IN_LIST consumers)
                continue()
            endif()
            set(dependencies)
            foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES SOURCES)
                get_target_property(links "${target}" "${property}")
                if(NOT links)
                    continue()
                endif()
                foreach(link IN LISTS links)
                    if(property STREQUAL "SOURCES")
                        if(link MATCHES "^\\$<TARGET_OBJECTS:([^<>]+)>$")
                            set(link "${CMAKE_MATCH_1}")
                        elseif(NOT link MATCHES "TARGET_OBJECTS")
                            continue()
                        endif()
                    endif()
                    # These wrappers carry a real target dependency. Unknown expressions require
                    # review rather than guessed semantics.
                    while(link MATCHES "^\\$<(LINK_ONLY|BUILD_INTERFACE):([^<>]+)>$")
                        set(link "${CMAKE_MATCH_2}")
                    endwhile()
                    if(link MATCHES "\\$<")
                        # Unrelated SDK conditionals are outside this policy. A hidden dependency on
                        # any discovered consumer needs explicit handling, including consumers found
                        # later.
                        foreach(consumer IN LISTS consumers)
                            string(FIND "${link}" "${consumer}" reference)
                            if(NOT reference EQUAL -1)
                                message(
                                    FATAL_ERROR
                                        "Review Hub header dependency expression on ${target}: ${link}"
                                )
                            endif()
                        endforeach()
                        continue()
                    endif()
                    if(TARGET "${link}")
                        get_target_property(alias "${link}" ALIASED_TARGET)
                        if(alias)
                            set(link "${alias}")
                        endif()
                        list(APPEND dependencies "${link}")
                    endif()
                endforeach()
            endforeach()
            foreach(dependency IN LISTS dependencies)
                if(dependency IN_LIST consumers)
                    list(APPEND consumers "${target}")
                    set(changed TRUE)
                    break()
                endif()
            endforeach()
        endforeach()
    endwhile()
    if("ladybird_impl" IN_LIST consumers)
        message(FATAL_ERROR "Standalone Ladybird must not consume the embedded Qt frontend")
    endif()
    foreach(target IN LISTS consumers)
        get_target_property(type "${target}" TYPE)
        if(type STREQUAL "INTERFACE_LIBRARY")
            target_include_directories("${target}" BEFORE INTERFACE "${overlay}")
        else()
            target_include_directories("${target}" BEFORE PRIVATE "${overlay}")
        endif()
    endforeach()
    set(required_targets)
    foreach(
        target
        firefly-qt-view
        firefly-application
        firefly-window-probe
        firefly-file-input-probe
        firefly-view-offscreen
        firefly-permission-boundary-probe
        firefly-background-probe
        firefly-window-constraints-probe
        firefly-spellcheck-probe)
        if(TARGET "${target}")
            if(NOT target IN_LIST consumers)
                message(FATAL_ERROR "Missing embedded Hub header policy on ${target}")
            endif()
            list(APPEND required_targets --require-target "${target}")
        endif()
    endforeach()
    if(CMAKE_EXPORT_COMPILE_COMMANDS)
        add_custom_target(
            check-firefly-hub-frontend-headers
            COMMAND
                "${Python3_EXECUTABLE}"
                "${FIREFLY_SOURCE_DIR}/tooling/check_hub_frontend_headers.py" --database
                "${CMAKE_BINARY_DIR}/compile_commands.json" --overlay "${overlay}" --upstream
                "${LADYBIRD_SOURCE_DIR}" ${required_targets}
            COMMAND "${Python3_EXECUTABLE}"
                    "${FIREFLY_SOURCE_DIR}/tooling/tests/test_hub_frontend_headers.py"
            VERBATIM)
        foreach(target IN LISTS consumers)
            add_dependencies("${target}" check-firefly-hub-frontend-headers)
        endforeach()
    endif()
    list(SORT consumers)
    message(STATUS "Embedded Hub header consumers: ${consumers}")
endfunction()

set_property(GLOBAL PROPERTY FIREFLY_HUB_FRONTEND_HEADERS "${hub_frontend_output}")
# Runtime/probe targets are declared after the Qt library, so inspect the complete graph once, at
# the end of this directory, before CMake generates build rules.
cmake_language(DEFER CALL firefly_apply_hub_header_policy)
