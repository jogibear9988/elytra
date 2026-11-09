if(TARGET firefly-view-offscreen)
    add_custom_target(
        check-firefly-web-request
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_web_request.py"
                --firefly "$<TARGET_FILE:firefly-view-offscreen>"
        DEPENDS firefly-view-offscreen check-firefly-web-request-native
        VERBATIM)
endif()
