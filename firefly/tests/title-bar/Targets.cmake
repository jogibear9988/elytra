# Native controls and full-size content are accepted only with the real Cocoa backend.
if(APPLE)
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/TitleBarProbe.mm" "${CMAKE_CURRENT_LIST_DIR}/RuntimeProbe.mm"
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    add_executable(
        firefly-title-bar-probe
        "${CMAKE_CURRENT_LIST_DIR}/TitleBarProbe.mm"
        "${FIREFLY_SOURCE_DIR}/firefly/native/qt/ChromeWindow.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/native/window/TitleBarMac.mm")
    target_link_libraries(firefly-title-bar-probe PRIVATE Qt::Widgets "-framework AppKit")
    add_executable(firefly-title-bar-runtime-probe "${CMAKE_CURRENT_LIST_DIR}/RuntimeProbe.mm")
    target_link_libraries(firefly-title-bar-runtime-probe PRIVATE firefly-application)
    add_dependencies(firefly-title-bar-runtime-probe ladybird ${ladybird_helper_processes}
                     check-firefly-upstream)
    ladybird_check_skia_confinement(firefly-title-bar-runtime-probe)
    add_custom_target(
        check-firefly-title-bar
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_title_bar.py"
            --firefly "$<TARGET_FILE:firefly>" --platform cocoa --probe
            "$<TARGET_FILE:firefly-title-bar-probe>" --runtime-probe
            "$<TARGET_FILE:firefly-title-bar-runtime-probe>"
        DEPENDS firefly firefly-title-bar-probe firefly-title-bar-runtime-probe
        VERBATIM)
endif()
