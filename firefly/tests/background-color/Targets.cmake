if(TARGET firefly-view-offscreen)
    add_executable(
        firefly-background-probe
        "${CMAKE_CURRENT_LIST_DIR}/NativeProbe.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/OffscreenPlugin.cpp")
    target_include_directories(firefly-background-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(firefly-background-probe PRIVATE firefly-application
                                                           Qt6::QOffscreenIntegrationPlugin)
    add_dependencies(firefly-background-probe ladybird ${ladybird_helper_processes}
                     check-firefly-upstream)
    ladybird_check_skia_confinement(firefly-background-probe)
    add_custom_target(
        check-firefly-background-color
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_background_color.py"
            --firefly "$<TARGET_FILE:firefly-view-offscreen>" --native-probe
            "$<TARGET_FILE:firefly-background-probe>"
        DEPENDS firefly-view-offscreen firefly-background-probe
        VERBATIM)
endif()
