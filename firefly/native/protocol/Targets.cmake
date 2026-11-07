# Only the renderer graph composes this policy; standalone Mite stays untouched.
set(protocol_origin_output "${CMAKE_BINARY_DIR}/firefly-adapters/protocol-origins")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_protocol_origin_hooks.py"
            --output "${protocol_origin_output}" RESULT_VARIABLE protocol_origin_check)
if(NOT protocol_origin_check EQUAL 0)
    message(FATAL_ERROR "Review the owned protocol origin adapters")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/prepare_protocol_origin_hooks.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibURL/URL.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebCommon/SecureContexts/AbstractOperations.cpp")

function(firefly_origin_source target original replacement)
    get_target_property(sources "${target}" SOURCES)
    get_target_property(source_directory "${target}" SOURCE_DIR)
    set(adapted)
    set(count 0)
    foreach(source IN LISTS sources)
        get_filename_component(absolute "${source}" ABSOLUTE BASE_DIR "${source_directory}")
        if(absolute STREQUAL "${LADYBIRD_SOURCE_DIR}/${original}")
            list(APPEND adapted "${protocol_origin_output}/${replacement}")
            math(EXPR count "${count}+1")
        else()
            list(APPEND adapted "${source}")
        endif()
    endforeach()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Review ${target} origin source layout")
    endif()
    set_property(TARGET "${target}" PROPERTY SOURCES "${adapted}")
    set_property(
        SOURCE "${protocol_origin_output}/${replacement}" TARGET_DIRECTORY "${target}"
        APPEND
        PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly")
endfunction()
firefly_origin_source(LibURL "Libraries/LibURL/URL.cpp" URL.cpp)
firefly_origin_source(LibWebCommon "Libraries/LibWebCommon/SecureContexts/AbstractOperations.cpp"
                      SecureContexts.cpp)
target_sources(LibURL PRIVATE "${CMAKE_CURRENT_LIST_DIR}/OriginPolicy.cpp")
add_custom_target(
    check-firefly-protocol-origin-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_protocol_origin_hooks.py"
            --output "${protocol_origin_output}"
    VERBATIM)
add_dependencies(LibURL check-firefly-protocol-origin-hooks)
add_dependencies(LibWebCommon check-firefly-protocol-origin-hooks)
add_executable(firefly-protocol-origin-probe "${CMAKE_CURRENT_LIST_DIR}/OriginProbe.cpp")
target_include_directories(firefly-protocol-origin-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-protocol-origin-probe PRIVATE LibURL LibWebCommon LibMain LibCore)
ladybird_copy_runtime_dlls(firefly-protocol-origin-probe)
add_custom_target(
    check-firefly-protocol-origins
    COMMAND "$<TARGET_FILE:firefly-protocol-origin-probe>" default
    COMMAND "$<TARGET_FILE:firefly-protocol-origin-probe>" configured
    DEPENDS firefly-protocol-origin-probe
    USES_TERMINAL VERBATIM)
