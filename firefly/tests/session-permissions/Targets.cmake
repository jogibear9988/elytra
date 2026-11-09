# Test frontends retain the exact application objects and use a static headless Qt plugin.
if(TARGET firefly-view-offscreen)
    add_executable(
        firefly-permission-boundary-probe
        "${CMAKE_CURRENT_LIST_DIR}/NativeBoundary.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/OffscreenPlugin.cpp")
    target_include_directories(firefly-permission-boundary-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(firefly-permission-boundary-probe
                          PRIVATE firefly-application Qt6::QOffscreenIntegrationPlugin)
    add_dependencies(firefly-permission-boundary-probe ladybird ${ladybird_helper_processes}
                     check-firefly-upstream)
    ladybird_check_skia_confinement(firefly-permission-boundary-probe)
    add_custom_target(
        check-firefly-permission-boundary
        COMMAND
            "${Python3_EXECUTABLE}"
            "${FIREFLY_SOURCE_DIR}/tooling/tests/test_permission_boundary.py" --probe
            "$<TARGET_FILE:firefly-permission-boundary-probe>"
        DEPENDS firefly-permission-boundary-probe
        VERBATIM)
    add_custom_target(
        check-firefly-session-permissions
        COMMAND
            "${Python3_EXECUTABLE}"
            "${FIREFLY_SOURCE_DIR}/tooling/tests/test_session_permissions.py" --firefly
            "$<TARGET_FILE:firefly-view-offscreen>"
        DEPENDS firefly-view-offscreen
        VERBATIM)
endif()
