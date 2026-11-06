# This module is included by Inject.cmake in an upstream-root build. It is never
# added to the standalone Mite build or linked into the Mite executable.
include("${FIREFLY_SOURCE_DIR}/mite/cmake/ExecutionContextAdapter.cmake")
mite_adapt_execution_context("${FIREFLY_SOURCE_DIR}")
if(ENABLE_LTO_FOR_RELEASE AND NOT CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    message(FATAL_ERROR "Requested Release LTO is unsupported by this compiler/linker")
endif()
add_executable(firefly-renderer-probe "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
target_link_libraries(firefly-renderer-probe PRIVATE LibWebView LibIPC LibURL LibGfx LibCore LibMain AK)
target_include_directories(firefly-renderer-probe PRIVATE "${LADYBIRD_SOURCE_DIR}" "${LADYBIRD_SOURCE_DIR}/Libraries")
target_compile_definitions(firefly-renderer-probe PRIVATE FIREFLY_LADYBIRD_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
add_dependencies(firefly-renderer-probe ladybird ${ladybird_helper_processes})
add_custom_target(check-firefly-upstream
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_upstream.py" VERBATIM)
add_dependencies(firefly-renderer-probe check-firefly-upstream)
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/CallbackTargets.cmake")
# Retain upstream's process boundary: Skia must stay in Compositor, including
# when our frontend is added after the upstream root's own confinement check.
ladybird_check_skia_confinement(firefly-renderer-probe)
add_custom_target(check-firefly-renderer
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/test_renderer.py"
        --probe "$<TARGET_FILE:firefly-renderer-probe>"
    DEPENDS firefly-renderer-probe
    USES_TERMINAL VERBATIM)

if(APPLE)
    include("${FIREFLY_SOURCE_DIR}/firefly/native/qt/Targets.cmake")
endif()

add_library(firefly-isolated-world STATIC "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/IsolatedWorld.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/BridgeData.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/FunctionBridge.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/WorldJobGuard.cpp")
target_include_directories(firefly-isolated-world PUBLIC "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge" "${LADYBIRD_SOURCE_DIR}" "${LADYBIRD_SOURCE_DIR}/Libraries")
target_link_libraries(firefly-isolated-world PUBLIC LibWeb LibJS LibGC LibCore LibURL AK)
add_dependencies(firefly-isolated-world check-firefly-upstream)
add_executable(firefly-isolation-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/main.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/BridgeDataTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/FunctionBridgeTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/PromiseBridgeTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/WorldJobGuardTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/HandlerTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/CustomEventTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/ErrorEventTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/MessageEventTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/PrincipalCallbackTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/NodeFilterTest.cpp")
target_link_libraries(firefly-isolation-probe PRIVATE firefly-isolated-world LibMain)
ladybird_check_skia_confinement(firefly-isolation-probe)
add_custom_target(check-firefly-isolation COMMAND "$<TARGET_FILE:firefly-isolation-probe>"
    DEPENDS firefly-isolation-probe USES_TERMINAL VERBATIM)

if(APPLE)
    include("${FIREFLY_SOURCE_DIR}/firefly/renderer-helper/Targets.cmake")
endif()

include("${FIREFLY_SOURCE_DIR}/mite/cmake/SizeOptimization.cmake")
elytra_enable_size_optimization()
