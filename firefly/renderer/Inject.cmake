# Loaded through CMAKE_PROJECT_ladybird_INCLUDE. Add our targets only after the unmodified upstream
# root has declared its libraries and helper processes.
if(NOT DEFINED FIREFLY_SOURCE_DIR)
    message(FATAL_ERROR "Configure with tooling/build_renderer.py")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(FIREFLY_CHECK_LADYBIRD_PATCH_STACK)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/ladybird_patches.py"
                --verify-source "${CMAKE_SOURCE_DIR}" RESULT_VARIABLE firefly_patch_check)
    if(NOT firefly_patch_check EQUAL 0)
        message(FATAL_ERROR "Prepare the Ladybird patch stack using tooling/build_renderer.py")
    endif()
    file(GLOB firefly_ladybird_patches CONFIGURE_DEPENDS
         "${FIREFLY_SOURCE_DIR}/patches/ladybird/*.patch")
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS ${firefly_ladybird_patches})
endif()

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
# Every SDK consumer must see the same broker class definition. Generate this opt-in Windows header
# before upstream subdirectories declare their targets.
if(WIN32 AND FIREFLY_EXPERIMENTAL_WINDOWS_HELPER_LAUNCH)
    set(FIREFLY_WINDOWS_BROKER_OVERLAY "${CMAKE_BINARY_DIR}/firefly-adapters/windows-broker")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/windows-sandbox/broker.py"
                --output "${FIREFLY_WINDOWS_BROKER_OVERLAY}"
        RESULT_VARIABLE firefly_windows_broker_result)
    if(NOT firefly_windows_broker_result EQUAL 0)
        message(FATAL_ERROR "Cannot compose the verified Windows handle broker")
    endif()
    set(FIREFLY_WINDOWS_HELPER_OVERLAY "${CMAKE_BINARY_DIR}/firefly-adapters/windows-helper")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/windows-sandbox/prepare.py"
                --output "${FIREFLY_WINDOWS_HELPER_OVERLAY}"
        RESULT_VARIABLE firefly_windows_helper_result)
    if(NOT firefly_windows_helper_result EQUAL 0)
        message(FATAL_ERROR "Cannot compose the verified Windows helper sources")
    endif()
    include_directories(BEFORE "${FIREFLY_WINDOWS_BROKER_OVERLAY}"
                        "${FIREFLY_WINDOWS_HELPER_OVERLAY}")
endif()

if(APPLE
   OR CMAKE_SYSTEM_NAME STREQUAL "Linux"
   OR (WIN32 AND FIREFLY_EXPERIMENTAL_WINDOWS_HELPER_LAUNCH))
    include("${FIREFLY_SOURCE_DIR}/firefly/renderer-helper/title-bar/MediaSnapshot.cmake")
endif()

cmake_language(DEFER CALL include "${FIREFLY_SOURCE_DIR}/firefly/renderer/Targets.cmake")

set(firefly_runtime_dependency_args)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND firefly_runtime_dependency_args --linux)
elseif(WIN32)
    list(APPEND firefly_runtime_dependency_args --windows)
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
