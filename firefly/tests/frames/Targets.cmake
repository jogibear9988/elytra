# Use the exact production application objects with the test-only static platform plugin.
if(TARGET firefly-view-offscreen)
    add_custom_target(
        check-firefly-frames
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_frames.py"
                --firefly "$<TARGET_FILE:firefly-view-offscreen>"
        DEPENDS firefly-view-offscreen
        VERBATIM)
endif()
