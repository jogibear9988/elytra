# One real renderer batch: native macOS objects use a test-only offscreen plugin; Linux keeps the
# production X11 adapter and is run under the hosted Xvfb display.
if(APPLE AND TARGET firefly-view-offscreen)
    set(firefly_view_test_runtime firefly-view-offscreen)
    set(firefly_view_test_platform offscreen)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(firefly_view_test_runtime firefly)
    set(firefly_view_test_platform xcb)
endif()
if(DEFINED firefly_view_test_runtime)
    add_custom_target(
        check-firefly-web-contents-view
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_web_contents_view.py"
            --firefly "$<TARGET_FILE:${firefly_view_test_runtime}>" --firefly-platform
            "${firefly_view_test_platform}"
        DEPENDS "${firefly_view_test_runtime}"
        WORKING_DIRECTORY "${FIREFLY_SOURCE_DIR}"
        VERBATIM)
else()
    add_custom_target(
        check-firefly-web-contents-view
        COMMAND "${CMAKE_COMMAND}" -E echo
                "Native view acceptance requires Qt's offscreen test plugin"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM)
endif()
