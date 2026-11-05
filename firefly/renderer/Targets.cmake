# This module is included by Inject.cmake in an upstream-root build. It is never
# added to the standalone Mite build or linked into the Mite executable.
add_executable(firefly-renderer-probe "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
target_link_libraries(firefly-renderer-probe PRIVATE LibWebView LibIPC LibURL LibGfx LibCore LibMain AK)
target_include_directories(firefly-renderer-probe PRIVATE "${LADYBIRD_SOURCE_DIR}" "${LADYBIRD_SOURCE_DIR}/Libraries")
target_compile_definitions(firefly-renderer-probe PRIVATE FIREFLY_LADYBIRD_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
add_dependencies(firefly-renderer-probe ladybird ${ladybird_helper_processes})
add_custom_target(check-firefly-upstream
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_upstream.py" VERBATIM)
add_dependencies(firefly-renderer-probe check-firefly-upstream)
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
