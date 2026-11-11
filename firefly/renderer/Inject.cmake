# Loaded through CMAKE_PROJECT_ladybird_INCLUDE. Add our targets only after the unmodified upstream
# root has declared its libraries and helper processes.
if(NOT DEFINED FIREFLY_SOURCE_DIR)
    message(FATAL_ERROR "Configure with tooling/build_renderer.py")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include("${FIREFLY_SOURCE_DIR}/mite/cmake/IcuData.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/protocol/RustOrigin.cmake")
cmake_path(CONVERT "${FIREFLY_SOURCE_DIR}/firefly/cmake" TO_CMAKE_PATH_LIST firefly_module_path
           NORMALIZE)
list(PREPEND CMAKE_MODULE_PATH "${firefly_module_path}")
set_property(GLOBAL PROPERTY FIREFLY_BYTE_GENERATOR_SELECTED FALSE)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_upstream.py"
                RESULT_VARIABLE firefly_upstream_check)
if(NOT firefly_upstream_check EQUAL 0)
    message(FATAL_ERROR "Firefly requires the synchronized, unmodified Ladybird pin")
endif()
cmake_language(DEFER CALL include "${FIREFLY_SOURCE_DIR}/firefly/renderer/Targets.cmake")

set(firefly_runtime_dependency_args)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND firefly_runtime_dependency_args --linux)
endif()
if(FIREFLY_BUILD_RUNTIME)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/firefly_dependencies.py"
                --check "${VCPKG_MANIFEST_DIR}" ${firefly_runtime_dependency_args}
        RESULT_VARIABLE firefly_dependencies_check)
    if(NOT firefly_dependencies_check EQUAL 0)
        message(
            FATAL_ERROR
                "Regenerate Firefly dependencies with tooling/build_renderer.py --target firefly")
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT FIREFLY_BUILD_RUNTIME)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/renderer_dependencies.py"
                --check "${VCPKG_MANIFEST_DIR}" RESULT_VARIABLE firefly_renderer_dependencies_check)
    if(NOT firefly_renderer_dependencies_check EQUAL 0)
        message(
            FATAL_ERROR
                "Regenerate owned Linux renderer dependencies with tooling/build_renderer.py")
    endif()
endif()
