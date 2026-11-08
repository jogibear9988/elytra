# Track native File creation without modifying the selected upstream checkout or its ABI.
set(file_origin_input "${CMAKE_BINARY_DIR}/firefly-adapters/HTMLInputElement.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_origin_hook.py"
            --output "${file_origin_input}" RESULT_VARIABLE file_origin_check)
if(NOT file_origin_check EQUAL 0)
    message(FATAL_ERROR "Review the native file-origin adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_origin_hook.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/HTMLInputElement.cpp")
get_target_property(file_origin_sources LibWeb SOURCES)
set(file_origin_replaced 0)
set(file_origin_adapted)
foreach(source IN LISTS file_origin_sources)
    if(source STREQUAL "HTML/HTMLInputElement.cpp"
       OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/HTMLInputElement.cpp")
        list(APPEND file_origin_adapted "${file_origin_input}")
        math(EXPR file_origin_replaced "${file_origin_replaced}+1")
    else()
        list(APPEND file_origin_adapted "${source}")
    endif()
endforeach()
if(NOT file_origin_replaced EQUAL 1)
    message(FATAL_ERROR "Review LibWeb input element source layout")
endif()
set_property(TARGET LibWeb PROPERTY SOURCES "${file_origin_adapted}")
target_sources(LibWeb PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Origins.cpp")
set_property(
    SOURCE "${file_origin_input}" "${CMAKE_CURRENT_LIST_DIR}/Origins.cpp" TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
add_custom_target(
    check-firefly-file-origin-hook
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_origin_hook.py"
            --output "${file_origin_input}"
    VERBATIM)
add_dependencies(LibWeb check-firefly-file-origin-hook)
