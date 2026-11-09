if(TARGET firefly-view-offscreen)
    add_executable(
        firefly-spellcheck-probe
        "${CMAKE_CURRENT_LIST_DIR}/NativeProbe.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/OffscreenPlugin.cpp")
    target_include_directories(firefly-spellcheck-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(firefly-spellcheck-probe PRIVATE firefly-application
                                                           Qt6::QOffscreenIntegrationPlugin)
    add_dependencies(firefly-spellcheck-probe ladybird ${ladybird_helper_processes}
                     check-firefly-upstream)
    ladybird_check_skia_confinement(firefly-spellcheck-probe)
    add_custom_target(
        check-firefly-spellcheck
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_spellcheck.py"
            --firefly "$<TARGET_FILE:firefly-view-offscreen>" --native-probe
            "$<TARGET_FILE:firefly-spellcheck-probe>"
        DEPENDS firefly-view-offscreen firefly-spellcheck-probe
        VERBATIM)
endif()
