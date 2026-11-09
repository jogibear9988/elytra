# Native click-through acceptance is macOS Cocoa only; every test window stays hidden.
if(APPLE)
    add_executable(
        firefly-first-mouse-probe "${CMAKE_CURRENT_LIST_DIR}/FirstMouseProbe.mm"
                                  "${FIREFLY_SOURCE_DIR}/firefly/native/window/FirstMouseMac.mm")
    target_compile_options(firefly-first-mouse-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-first-mouse-probe PRIVATE Qt::Widgets "-framework AppKit")
    add_custom_target(
        check-firefly-window-input
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_window_input.py"
            --firefly "$<TARGET_FILE:firefly>" --platform cocoa --probe
            "$<TARGET_FILE:firefly-first-mouse-probe>"
        DEPENDS firefly firefly-first-mouse-probe
        VERBATIM)
endif()
