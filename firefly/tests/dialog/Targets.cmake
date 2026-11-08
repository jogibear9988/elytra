# Headless exact Host acceptance shares the real VM and Qt event delivery, without native panels.
if(TARGET Qt6::QOffscreenIntegrationPlugin)
    add_executable(
        firefly-dialog-message-widget-probe
        "${CMAKE_CURRENT_LIST_DIR}/MessageWidgetProbe.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/native/dialog/Provider.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/native/dialog/FilesUnsupported.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/OffscreenPlugin.cpp")
    target_compile_definitions(firefly-dialog-message-widget-probe
                               PRIVATE FIREFLY_DIALOG_PROVIDER_TESTING)
    target_include_directories(firefly-dialog-message-widget-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(firefly-dialog-message-widget-probe
                          PRIVATE Qt::Widgets Qt6::QOffscreenIntegrationPlugin Threads::Threads)
    add_custom_target(
        check-firefly-dialog-message-widget
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_dialog_widget.py"
                --probe "$<TARGET_FILE:firefly-dialog-message-widget-probe>"
        DEPENDS firefly-dialog-message-widget-probe
        VERBATIM)
    add_executable(
        firefly-dialog-provider-probe
        "${CMAKE_CURRENT_LIST_DIR}/ProviderProbe.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/native/dialog/Provider.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/native/dialog/FilesUnsupported.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/OffscreenPlugin.cpp")
    target_compile_definitions(firefly-dialog-provider-probe
                               PRIVATE FIREFLY_DIALOG_PROVIDER_TESTING)
    target_include_directories(firefly-dialog-provider-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(firefly-dialog-provider-probe
                          PRIVATE Qt::Widgets Qt6::QOffscreenIntegrationPlugin Threads::Threads)
    add_custom_target(
        check-firefly-dialog-provider
        COMMAND "$<TARGET_FILE:firefly-dialog-provider-probe>"
        DEPENDS firefly-dialog-provider-probe
        VERBATIM)
    add_custom_target(
        check-firefly-dialog-public
        COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
                "$<TARGET_FILE:firefly-view-offscreen>" "${CMAKE_CURRENT_LIST_DIR}/public.mjs"
        DEPENDS firefly-view-offscreen
        VERBATIM)
    add_executable(firefly-dialog-host-probe "${CMAKE_CURRENT_LIST_DIR}/HostProbe.cpp"
                                             "${FIREFLY_SOURCE_DIR}/firefly/native/dialog/Host.cpp")
    target_include_directories(firefly-dialog-host-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(
        firefly-dialog-host-probe PRIVATE firefly-mite Qt::Widgets Qt6::QOffscreenIntegrationPlugin
                                          Threads::Threads)
    add_custom_target(
        check-firefly-dialog-host
        COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
                "$<TARGET_FILE:firefly-dialog-host-probe>" "${CMAKE_CURRENT_LIST_DIR}/host.mjs"
        COMMAND
            "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
            "$<TARGET_FILE:firefly-dialog-host-probe>" "${CMAKE_CURRENT_LIST_DIR}/host.mjs"
            --leave-active
        DEPENDS firefly-dialog-host-probe
        VERBATIM)
endif()

if(APPLE)
    add_executable(firefly-dialog-native-probe "${CMAKE_CURRENT_LIST_DIR}/NativeProbe.mm")
    target_include_directories(firefly-dialog-native-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-dialog-native-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-dialog-native-probe PRIVATE firefly-dialog Threads::Threads)
    # Opt-in only: actual UI must never run during ordinary CI or source checks.
    add_custom_target(
        check-firefly-dialog-native
        COMMAND "$<TARGET_FILE:firefly-dialog-native-probe>" --ui-batch
        DEPENDS firefly-dialog-native-probe
        VERBATIM)
endif()
