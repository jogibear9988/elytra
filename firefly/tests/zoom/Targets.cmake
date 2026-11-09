if(TARGET firefly-view-offscreen)
    add_custom_target(
        check-firefly-zoom
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_zoom.py" --firefly
                "$<TARGET_FILE:firefly-view-offscreen>"
        DEPENDS firefly-view-offscreen
        VERBATIM)
endif()
