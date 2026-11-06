# Native OS-service acceptance must not wait for LibWeb or embed another JS runtime.
cmake_minimum_required(VERSION 3.30)
project(FireflyNativeServices LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
get_filename_component(FIREFLY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_upstream.py"
                RESULT_VARIABLE upstream_status)
if(NOT upstream_status EQUAL 0)
    message(
        FATAL_ERROR "Native service acceptance requires the synchronized unmodified upstream pin")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/renderer_dependencies.py"
                --services --check "${VCPKG_MANIFEST_DIR}" RESULT_VARIABLE manifest_status)
    if(NOT manifest_status EQUAL 0)
        message(FATAL_ERROR "Configure with the generated standalone native-service manifest")
    endif()
elseif(NOT APPLE)
    message(FATAL_ERROR "Standalone native-service acceptance currently supports macOS/Linux only")
endif()
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui GuiPrivate Widgets)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_SOURCE_DIR}/qt/Requirements.cmake")
    firefly_require_wayland_native_interfaces()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/clipboard/Targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/window/Targets.cmake")
enable_testing()
add_test(NAME clipboard-protocol COMMAND firefly-clipboard-protocol-probe)
add_test(
    NAME clipboard-isolation-guard
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_x11_clipboard.py"
            --probe "$<TARGET_FILE:firefly-x11-clipboard-probe>" --check-guard)
set_tests_properties(clipboard-protocol clipboard-isolation-guard PROPERTIES TIMEOUT 15)
add_test(NAME geometry-arithmetic COMMAND firefly-native-geometry-probe)
add_test(NAME geometry-isolation-guard
         COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_x11_geometry.py"
                 --probe "$<TARGET_FILE:firefly-x11-geometry-probe>" --check-guard)
set_tests_properties(geometry-arithmetic geometry-isolation-guard PROPERTIES TIMEOUT 15)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(
        NAME clipboard-native-x11
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_x11_clipboard.py"
                --probe "$<TARGET_FILE:firefly-x11-clipboard-probe>")
    set_tests_properties(clipboard-native-x11 PROPERTIES TIMEOUT 90)
    add_test(
        NAME geometry-native-x11
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_x11_geometry.py"
                --probe "$<TARGET_FILE:firefly-x11-geometry-probe>")
    set_tests_properties(geometry-native-x11 PROPERTIES TIMEOUT 90)
endif()
