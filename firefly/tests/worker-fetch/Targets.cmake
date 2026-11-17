# The locally accepted dedicated-worker gate uses the exact offscreen app objects.
if(APPLE AND TARGET firefly-view-offscreen)
    add_custom_target(
        check-firefly-worker-fetch
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_worker_fetch.py"
                --firefly "$<TARGET_FILE:firefly-view-offscreen>"
        DEPENDS firefly-view-offscreen check-firefly-worker-hooks
        VERBATIM)
endif()
