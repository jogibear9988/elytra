if(TARGET firefly-view-offscreen)
    add_executable(
        firefly-window-constraints-probe
        "${CMAKE_CURRENT_LIST_DIR}/NativeProbe.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/OffscreenPlugin.cpp")
    target_include_directories(firefly-window-constraints-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(firefly-window-constraints-probe PRIVATE firefly-application
                                                                   Qt6::QOffscreenIntegrationPlugin)
    add_dependencies(firefly-window-constraints-probe ladybird ${ladybird_helper_processes}
                     check-firefly-upstream)
    ladybird_check_skia_confinement(firefly-window-constraints-probe)
    add_custom_target(
        check-firefly-window-constraints
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_window_constraints.py"
            --firefly "$<TARGET_FILE:firefly-view-offscreen>" --native-probe
            "$<TARGET_FILE:firefly-window-constraints-probe>"
        DEPENDS firefly-view-offscreen firefly-window-constraints-probe
        VERBATIM)
endif()
