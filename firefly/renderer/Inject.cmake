# Loaded through CMAKE_PROJECT_ladybird_INCLUDE. Add our targets only after the
# unmodified upstream root has declared its libraries and helper processes.
if(NOT DEFINED FIREFLY_SOURCE_DIR)
    message(FATAL_ERROR "Configure with tooling/build_renderer.py")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_upstream.py"
    RESULT_VARIABLE firefly_upstream_check)
if(NOT firefly_upstream_check EQUAL 0)
    message(FATAL_ERROR "Firefly requires the synchronized, unmodified Ladybird pin")
endif()
cmake_language(DEFER CALL include "${FIREFLY_SOURCE_DIR}/firefly/renderer/Targets.cmake")
