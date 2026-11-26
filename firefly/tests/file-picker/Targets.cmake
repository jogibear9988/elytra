if(APPLE)
    # Reuse the production application with a test-only hidden native chooser controller.
    add_executable(firefly-file-access-probe "${CMAKE_CURRENT_LIST_DIR}/AccessViewProbe.cpp")
    target_include_directories(firefly-file-access-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(firefly-file-access-probe PRIVATE firefly-application)
    ladybird_check_skia_confinement(firefly-file-access-probe)
    add_custom_target(
        check-firefly-file-access
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_file_access.py"
                --probe "$<TARGET_FILE:firefly-file-access-probe>"
        DEPENDS firefly-file-access-probe
        VERBATIM)
endif()
