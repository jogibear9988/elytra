# Replace exactly one upstream translation unit with the checked composition.
set(file_module_fetch "${CMAKE_BINARY_DIR}/firefly-adapters/Fetching.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_module_hook.py"
            --output "${file_module_fetch}" RESULT_VARIABLE file_module_check)
if(NOT file_module_check EQUAL 0)
    message(FATAL_ERROR "Review the owned file module loading adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_module_hook.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/Fetch/Fetching/Fetching.cpp")
get_target_property(file_module_sources LibWeb SOURCES)
set(file_module_replaced 0)
set(file_module_adapted)
foreach(source IN LISTS file_module_sources)
    if(source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/Fetch/Fetching/Fetching.cpp")
        list(APPEND file_module_adapted "${file_module_fetch}")
        math(EXPR file_module_replaced "${file_module_replaced}+1")
    else()
        list(APPEND file_module_adapted "${source}")
    endif()
endforeach()
if(NOT file_module_replaced EQUAL 1)
    message(FATAL_ERROR "Review LibWeb fetch source layout")
endif()
set_property(TARGET LibWeb PROPERTY SOURCES "${file_module_adapted}")
target_sources(LibWeb PRIVATE "${CMAKE_CURRENT_LIST_DIR}/FileModules.cpp")
set_property(
    SOURCE "${file_module_fetch}" "${CMAKE_CURRENT_LIST_DIR}/FileModules.cpp" TARGET_DIRECTORY
           LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
add_custom_target(
    check-firefly-file-module-hook
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_module_hook.py"
            --output "${file_module_fetch}"
    VERBATIM)
add_dependencies(LibWeb check-firefly-file-module-hook)
