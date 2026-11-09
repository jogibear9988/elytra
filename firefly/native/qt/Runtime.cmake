# Select real native adapters; the Linux runtime remains experimental until its hosted gate passes.
if(NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "No native Firefly desktop runtime for this platform")
endif()
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-helper/Layout.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../platform/RuntimePaths.cmake")
add_library(firefly-application-menu STATIC "${CMAKE_CURRENT_LIST_DIR}/../menu/ApplicationMenu.cpp")
target_link_libraries(firefly-application-menu PUBLIC Qt::Widgets)
add_library(firefly-app-paths STATIC "${CMAKE_CURRENT_LIST_DIR}/../app-paths/Paths.cpp")
target_link_libraries(firefly-app-paths PUBLIC Qt::Core LibCore AK)
add_library(firefly-sandbox-policy STATIC "${CMAKE_CURRENT_LIST_DIR}/../sandbox/policy/Policy.cpp")
add_library(firefly-command-line STATIC "${CMAKE_CURRENT_LIST_DIR}/../command-line/State.cpp")
add_library(firefly-tracing STATIC "${CMAKE_CURRENT_LIST_DIR}/../tracing/Recorder.cpp")
target_link_libraries(firefly-tracing PUBLIC Qt::Core)
if(APPLE)
    add_library(firefly-locale STATIC "${CMAKE_CURRENT_LIST_DIR}/../locale/LocaleMac.mm")
    target_link_libraries(firefly-locale PUBLIC Qt::Core "-framework Foundation")
else()
    add_library(firefly-locale STATIC "${CMAKE_CURRENT_LIST_DIR}/../locale/LocaleLinux.cpp")
    target_link_libraries(firefly-locale PUBLIC Qt::Core)
endif()
add_custom_target(
    check-firefly-js
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_firefly_js.py"
    VERBATIM)
include("${FIREFLY_SOURCE_DIR}/mite/cmake/Runtime.cmake")
mite_add_runtime(firefly-mite "${FIREFLY_SOURCE_DIR}" LibJS)
include("${CMAKE_CURRENT_LIST_DIR}/../safe-storage/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/desktop-capture/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/notifications/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/power-monitor/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/power-save-blocker/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/shell/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/system-preferences/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/net/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/global-shortcut/Targets.cmake")
add_custom_target(
    check-firefly-dependencies
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/firefly_dependencies.py" --check
            "${VCPKG_MANIFEST_DIR}" ${firefly_runtime_dependency_args}
    VERBATIM)
add_dependencies(firefly-mite check-firefly-dependencies)
add_library(firefly-protocol-routes STATIC "${CMAKE_CURRENT_LIST_DIR}/../protocol/routes/Files.cpp"
                                           "${CMAKE_CURRENT_LIST_DIR}/../protocol/routes/Host.cpp")
target_link_libraries(firefly-protocol-routes PUBLIC firefly-protocol-file-server LibWebView LibJS)
add_executable(firefly-protocol-route-host-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/RouteHostProbe.cpp")
target_link_libraries(firefly-protocol-route-host-probe PRIVATE firefly-mite
                                                                firefly-protocol-routes)
ladybird_check_skia_confinement(firefly-protocol-route-host-probe)
if(WIN32)
    ladybird_windows_bin(firefly-protocol-route-host-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-protocol-route-host
    COMMAND "$<TARGET_FILE:firefly-protocol-route-host-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/route-host.mjs"
    DEPENDS firefly-protocol-route-host-probe
    VERBATIM)
# Main-origin registration uses LibJS/LibURL only; its acceptance must not need Qt windows.
add_executable(
    firefly-protocol-registration-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/RegistrationProbe.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../protocol/RegistrationHost.cpp")
target_include_directories(firefly-protocol-registration-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-protocol-registration-probe PRIVATE firefly-mite LibURL)
add_custom_target(
    check-firefly-protocol-registration
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/registration.mjs" publish
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/registration.mjs" empty
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/registration.mjs" ready
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/registration.mjs" sealed
    DEPENDS firefly-protocol-registration-probe
    VERBATIM)

add_executable(firefly-menu-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/menu/NativeProbe.cpp"
                                  "${CMAKE_CURRENT_LIST_DIR}/../menu/Host.cpp")
target_link_libraries(firefly-menu-probe PRIVATE firefly-mite firefly-application-menu)
target_include_directories(firefly-menu-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
add_executable(firefly-theme-settings-probe "${CMAKE_CURRENT_LIST_DIR}/../theme/SettingsProbe.cpp"
                                            "${CMAKE_CURRENT_LIST_DIR}/../theme/ThemeSettings.cpp")
target_include_directories(firefly-theme-settings-probe
                           PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../theme")
target_link_libraries(firefly-theme-settings-probe PRIVATE LibWebView LibCore LibURL AK)
add_custom_target(
    check-firefly-theme-settings
    COMMAND "$<TARGET_FILE:firefly-theme-settings-probe>"
    DEPENDS firefly-theme-settings-probe
    VERBATIM)
add_custom_target(
    check-firefly-menu
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/menu/policy.mjs"
    COMMAND "$<TARGET_FILE:firefly-menu-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/menu/native.mjs"
    DEPENDS firefly-menu-probe firefly-protocol-registration-probe
    VERBATIM)
add_executable(firefly-app-paths-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/app-paths/Policy.cpp")
target_include_directories(firefly-app-paths-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-app-paths-probe PRIVATE firefly-app-paths)
add_custom_target(
    check-firefly-app-paths-native
    COMMAND "$<TARGET_FILE:firefly-app-paths-probe>"
    DEPENDS firefly-app-paths-probe
    VERBATIM)
add_executable(firefly-sandbox-policy-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/sandbox/Policy.cpp")
target_include_directories(firefly-sandbox-policy-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-sandbox-policy-probe PRIVATE firefly-sandbox-policy)
add_custom_target(
    check-firefly-sandbox-policy
    COMMAND "$<TARGET_FILE:firefly-sandbox-policy-probe>"
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/sandbox/policy.mjs"
    DEPENDS firefly-sandbox-policy-probe firefly-protocol-registration-probe
    VERBATIM)
add_executable(firefly-command-line-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/command-line/State.cpp")
target_include_directories(firefly-command-line-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-command-line-probe PRIVATE firefly-command-line)
add_custom_target(
    check-firefly-command-line-native
    COMMAND "$<TARGET_FILE:firefly-command-line-probe>"
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/command-line/policy.mjs"
    DEPENDS firefly-command-line-probe firefly-protocol-registration-probe
    VERBATIM)
add_executable(firefly-tracing-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/tracing/Recorder.cpp")
target_include_directories(firefly-tracing-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-tracing-probe PRIVATE firefly-tracing)
add_custom_target(
    check-firefly-tracing-native
    COMMAND "$<TARGET_FILE:firefly-tracing-probe>"
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/tracing/policy.mjs"
    DEPENDS firefly-tracing-probe firefly-protocol-registration-probe
    VERBATIM)
file(READ "${FIREFLY_SOURCE_DIR}/firefly/core/generated/utility-parent.js"
     FIREFLY_UTILITY_PARENT_SOURCE)
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/firefly/core/generated/utility-parent.js")
configure_file("${CMAKE_CURRENT_LIST_DIR}/../utility/UtilityParentSource.h.in"
               "${CMAKE_BINARY_DIR}/firefly-generated/UtilityParentSource.h" @ONLY)
file(READ "${FIREFLY_SOURCE_DIR}/firefly/core/generated/utility-net.js" FIREFLY_UTILITY_NET_SOURCE)
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/firefly/core/generated/utility-net.js")
configure_file("${CMAKE_CURRENT_LIST_DIR}/../net/UtilitySource.h.in"
               "${CMAKE_BINARY_DIR}/firefly-generated/UtilityNetSource.h" @ONLY)
file(READ "${FIREFLY_SOURCE_DIR}/firefly/core/generated/safe-storage.js"
     FIREFLY_SAFE_STORAGE_SOURCE)
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/firefly/core/generated/safe-storage.js")
configure_file("${CMAKE_CURRENT_LIST_DIR}/../safe-storage/SafeStorageSource.h.in"
               "${CMAKE_BINARY_DIR}/firefly-generated/SafeStorageSource.h" @ONLY)
file(READ "${FIREFLY_SOURCE_DIR}/firefly/core/generated/api.js" FIREFLY_API_SOURCE)
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/firefly/core/generated/api.js")
configure_file("${FIREFLY_SOURCE_DIR}/firefly/core/ApiSource.h.in"
               "${CMAKE_BINARY_DIR}/firefly-generated/ApiSource.h" @ONLY)
file(READ "${FIREFLY_SOURCE_DIR}/firefly/core/generated/app-regions.js" FIREFLY_APP_REGIONS_SOURCE)
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/firefly/core/generated/app-regions.js")
configure_file("${CMAKE_CURRENT_LIST_DIR}/AppRegions.h.in"
               "${CMAKE_BINARY_DIR}/firefly-generated/AppRegions.h" @ONLY)
set(firefly_native_services)
if(APPLE)
    list(
        APPEND
        firefly_native_services
        "${CMAKE_CURRENT_LIST_DIR}/../platform/Mac.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../clipboard/ClipboardMac.mm"
        "${CMAKE_CURRENT_LIST_DIR}/../theme/ThemeHost.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../theme/ThemeMac.mm"
        "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenHost.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenMac.mm"
        "${CMAKE_CURRENT_LIST_DIR}/../window/GeometryMac.mm"
        "${CMAKE_CURRENT_LIST_DIR}/../window/FirstMouseMac.mm"
        "${CMAKE_CURRENT_LIST_DIR}/../window/TitleBarMac.mm")
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../window/FirstMouseMac.mm"
                                PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../window/TitleBarMac.mm"
                                PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
else()
    list(APPEND firefly_native_services "${CMAKE_CURRENT_LIST_DIR}/../window/GeometryLinux.cpp"
         "${CMAKE_CURRENT_LIST_DIR}/../window/FirstMouseUnsupported.cpp"
         "${CMAKE_CURRENT_LIST_DIR}/../window/TitleBarUnsupported.cpp")
endif()
# Compile startup refusal without a GUI/VM, including on macOS; native X11 bindings stay Linux-only.
add_library(firefly-linux-environment STATIC
            "${CMAKE_CURRENT_LIST_DIR}/../platform/LinuxEnvironment.cpp")
target_link_libraries(firefly-linux-environment PUBLIC AK Qt::Core)
add_executable(firefly-linux-environment-probe
               "${CMAKE_CURRENT_LIST_DIR}/../platform/EnvironmentProbe.cpp")
target_link_libraries(firefly-linux-environment-probe PRIVATE firefly-linux-environment)
if(NOT APPLE)
    add_library(
        firefly-linux-desktop-services STATIC
        "${CMAKE_CURRENT_LIST_DIR}/../platform/Linux.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenHost.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenLinux.cpp")
    target_link_libraries(
        firefly-linux-desktop-services PUBLIC LibJS LibGC AK firefly-linux-environment
                                              firefly-x11-screen firefly-screen-events)
endif()
add_library(
    firefly-application OBJECT
    "${CMAKE_CURRENT_LIST_DIR}/Application.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../theme/ThemeSettings.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../gpu/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../utility/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../safe-storage/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../utility/Bootstrap.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Bridge.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ContentView.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Background.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Constraints.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../window/Constraints.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Lifecycle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Ipc.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/IpcChannel.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/OutboundIpc.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Preload.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/PreloadProcess.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Protocols.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Permissions.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Frames.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../frames/Registry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/WebRequest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../web-request/Broker.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Windows.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Views.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../desktop-capture/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../notifications/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../power-monitor/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../power-save-blocker/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../shell/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../system-preferences/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../net/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../global-shortcut/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../dialog/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../clipboard/ClipboardHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../crash/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../menu/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../tracing/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../sandbox/policy/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../command-line/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../app-paths/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../locale/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../command-line/EnginePolicy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../protocol/RegistrationHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../window/Geometry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../window/FrameGeometry.cpp"
    ${firefly_native_services}
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeData.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeWire.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/WireValue.cpp")
target_include_directories(
    firefly-application PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge"
                                "${FIREFLY_SOURCE_DIR}/firefly/native/ipc")
target_include_directories(firefly-application PRIVATE "${CMAKE_BINARY_DIR}/firefly-generated")
target_compile_definitions(firefly-application
                           PRIVATE FIREFLY_LADYBIRD_DIRECTORY="${firefly_runtime_binary_dir}")
target_link_libraries(
    firefly-application
    PUBLIC firefly-qt-view
           firefly-mite
           firefly-clipboard-protocol
           firefly-runtime-paths
           firefly-screen-events
           firefly-desktop-capture
           firefly-notifications
           firefly-power-monitor
           firefly-power-save-blocker
           firefly-shell
           firefly-system-preferences
           firefly-network-state
           firefly-global-shortcut
           firefly-dialog
           firefly-gpu-policy
           firefly-protocol-routes
           firefly-byte-channel
           firefly-web-request-wire
           firefly-preload-config
           firefly-file-picker
           firefly-application-menu
           firefly-tracing
           firefly-command-line
           firefly-sandbox-policy
           firefly-app-paths
           firefly-locale
           firefly-safe-storage)
if(NOT APPLE)
    target_link_libraries(firefly-application PUBLIC firefly-linux-desktop-services
                                                     firefly-x11-clipboard)
endif()
# Compile the application/adapters once. The offscreen acceptance frontend uses these exact objects;
# its only extra code imports the test-only static Qt plugin.
add_dependencies(firefly-application check-firefly-js check-firefly-upstream)
add_executable(firefly)
target_link_libraries(firefly PRIVATE firefly-application)
add_dependencies(firefly ladybird ${ladybird_helper_processes} check-firefly-upstream)
ladybird_check_skia_confinement(firefly)

if(NOT TARGET Qt6::QOffscreenIntegrationPlugin)
    find_package(Qt6QOffscreenIntegrationPlugin QUIET PATHS "${Qt6Gui_DIR}" NO_DEFAULT_PATH)
endif()
if(TARGET Qt6::QOffscreenIntegrationPlugin)
    add_executable(firefly-view-offscreen
                   "${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/OffscreenPlugin.cpp")
    target_link_libraries(firefly-view-offscreen PRIVATE firefly-application
                                                         Qt6::QOffscreenIntegrationPlugin)
    add_dependencies(firefly-view-offscreen ladybird ${ladybird_helper_processes}
                     check-firefly-upstream)
    ladybird_check_skia_confinement(firefly-view-offscreen)
    add_executable(
        firefly-menu-offscreen-probe
        "${FIREFLY_SOURCE_DIR}/firefly/tests/menu/NativeProbe.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../menu/Host.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/OffscreenPlugin.cpp")
    target_include_directories(firefly-menu-offscreen-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(
        firefly-menu-offscreen-probe PRIVATE firefly-mite firefly-application-menu
                                             Qt6::QOffscreenIntegrationPlugin)
    add_custom_target(
        check-firefly-menu-offscreen
        COMMAND
            "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME=generic
            "$<TARGET_FILE:firefly-menu-offscreen-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/menu/native.mjs" --offscreen
        DEPENDS firefly-menu-offscreen-probe
        VERBATIM)

    add_custom_target(
        check-firefly-global-shortcut-public
        COMMAND
            "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
            "$<TARGET_FILE:firefly-view-offscreen>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/global-shortcut/public.mjs"
        DEPENDS firefly-view-offscreen
        VERBATIM)
else()
    message(
        STATUS "Qt offscreen static plugin unavailable: headless Firefly view frontend disabled")
endif()
# Prefer the exact application objects with a test-only static platform plugin. Without that plugin,
# hosted native/Xvfb acceptance supplies the desktop backend.
set(fork_driver firefly)
set(fork_driver_options)
if(TARGET firefly-view-offscreen)
    set(fork_driver firefly-view-offscreen)
    list(APPEND fork_driver_options --offscreen)
endif()
include("${FIREFLY_SOURCE_DIR}/firefly/tests/dialog/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/gpu/Targets.cmake")
add_custom_target(
    check-firefly-fork
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_firefly_fork.py"
            --firefly "$<TARGET_FILE:${fork_driver}>" ${fork_driver_options}
    DEPENDS ${fork_driver}
    VERBATIM)
if(APPLE)
    add_custom_target(
        check-firefly-net-transport
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_firefly_net.py"
            --firefly "$<TARGET_FILE:${fork_driver}>" ${fork_driver_options} --native-probe
            "$<TARGET_FILE:firefly-net-native-probe>"
        DEPENDS ${fork_driver} firefly-net-native-probe
        VERBATIM)
endif()
add_custom_target(
    check-firefly-protocol-host
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_protocol_registration.py"
        --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)

add_executable(firefly-locale-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/locale/NativeProbe.cpp"
                                    "${CMAKE_CURRENT_LIST_DIR}/../locale/Host.cpp")
target_include_directories(firefly-locale-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-locale-probe PRIVATE firefly-mite firefly-locale)
add_custom_target(
    check-firefly-ipc-events
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/ipc/events/policy.mjs"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_ipc_events.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly firefly-protocol-registration-probe
    VERBATIM)

add_custom_target(
    check-firefly-ipc-channel
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_ipc_channel_binding.py"
        --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)

add_custom_target(
    check-firefly-preload-zoom
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_preload_zoom.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)

add_custom_target(
    check-firefly-preload-process
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_preload_process.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)

add_custom_target(
    check-firefly-preload-getters
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_preload_getters.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)

add_custom_target(
    check-firefly-ipc-binary
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_ipc_binary.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)

add_custom_target(
    check-firefly-ipc-outbound
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_ipc_outbound.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)

add_custom_target(
    check-firefly-app-locale
    COMMAND "$<TARGET_FILE:firefly-locale-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/locale/native.mjs"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_app_locale.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly firefly-locale-probe
    VERBATIM)

add_custom_target(
    check-firefly-app-paths
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_app_paths.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly check-firefly-app-paths-native
    VERBATIM)

add_custom_target(
    check-firefly-enable-sandbox
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_enable_sandbox.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly check-firefly-sandbox-policy
    VERBATIM)

add_custom_target(
    check-firefly-command-line
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_command_line.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly check-firefly-command-line-native
    VERBATIM)

add_custom_target(
    check-firefly-content-tracing
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_content_tracing.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly check-firefly-tracing-native
    VERBATIM)

add_custom_target(
    check-firefly-menu-host
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_application_menu.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)

add_custom_target(
    check-firefly-public-protocol
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_public_protocol.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)
add_custom_target(
    check-firefly-modern-protocol
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_modern_protocol.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    VERBATIM)
add_dependencies(check-firefly-public-protocol check-firefly-modern-protocol)
add_custom_target(
    check-firefly-file-policy
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/file-policy.mjs"
    DEPENDS firefly-protocol-registration-probe
    VERBATIM)
add_dependencies(check-firefly-protocol-registration check-firefly-file-policy)
add_dependencies(check-firefly-protocol-registration check-firefly-protocol-route-host)
add_custom_target(
    check-firefly-crash-reporter
    COMMAND "$<TARGET_FILE:firefly-protocol-registration-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/crash/policy.mjs"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_crash_reporter.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly firefly-protocol-registration-probe
    VERBATIM)
if(APPLE)
    set(FIREFLY_ELECTRON_REFERENCE
        "${FIREFLY_SOURCE_DIR}/build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron"
        CACHE FILEPATH "Electron behavioral reference executable")
    add_custom_target(
        check-firefly-original-fs
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_original_fs.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        DEPENDS firefly
        USES_TERMINAL VERBATIM)
    add_custom_target(
        check-firefly-electron-compat
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_electron_compat.py"
            --firefly "$<TARGET_FILE:firefly>"
        DEPENDS firefly
        USES_TERMINAL VERBATIM)
    add_custom_target(
        check-firefly
        COMMAND "$<TARGET_FILE:firefly-clipboard-protocol-probe>"
        COMMAND "$<TARGET_FILE:firefly-chrome-probe>"
        COMMAND "$<TARGET_FILE:firefly-screen-probe>"
        COMMAND "$<TARGET_FILE:firefly-clipboard-probe>"
        COMMAND "$<TARGET_FILE:firefly-theme-probe>"
        COMMAND "$<TARGET_FILE:firefly-geometry-probe>"
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_electron_compat.py"
            --firefly "$<TARGET_FILE:firefly>"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_original_fs.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_firefly.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_public_preload.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_renderer_modules.py"
            --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_public_ipc.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_ipc_events.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_ipc_outbound.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        DEPENDS firefly
                check-firefly-protocol-origins
                check-firefly-protocol-files
                check-firefly-protocol-fetch
                check-firefly-protocol-registration
                check-firefly-protocol-host
                check-firefly-menu
                check-firefly-menu-host
                check-firefly-content-tracing
                check-firefly-command-line
                check-firefly-enable-sandbox
                check-firefly-app-paths
                check-firefly-app-locale
                check-firefly-ipc-events
                check-firefly-ipc-outbound
                check-firefly-ipc-channel
                check-firefly-ipc-binary
                check-firefly-message-ports
                check-firefly-message-port-lifetime
                check-firefly-utility-process
                check-firefly-web-contents-view
                check-firefly-safe-storage-backend
                check-firefly-safe-storage-public
                check-firefly-file-picker
                check-firefly-file-input
                check-firefly-preload-zoom
                check-firefly-preload-process
                check-firefly-preload-getters
                check-firefly-crash-reporter
                check-firefly-public-protocol
                check-firefly-protocol-renderer
                check-firefly-resource-renderer
                firefly-chrome-probe
                firefly-screen-probe
                firefly-clipboard-probe
                firefly-clipboard-protocol-probe
                firefly-theme-probe
                firefly-geometry-probe
        USES_TERMINAL VERBATIM)
endif()

include("${FIREFLY_SOURCE_DIR}/firefly/tests/utility-process/Targets.cmake")

add_executable(
    firefly-safe-storage-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/safe-storage/PublicProbe.cpp"
                               "${CMAKE_CURRENT_LIST_DIR}/../safe-storage/Host.cpp")
target_include_directories(
    firefly-safe-storage-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly"
                                       "${CMAKE_BINARY_DIR}/firefly-generated")
target_link_libraries(firefly-safe-storage-probe PRIVATE firefly-mite firefly-safe-storage)
add_dependencies(firefly-safe-storage-probe check-firefly-js)

add_custom_target(
    check-firefly-safe-storage-public
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_safe_storage.py"
            --probe "$<TARGET_FILE:firefly-safe-storage-probe>" --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly firefly-safe-storage-probe
    WORKING_DIRECTORY "${FIREFLY_SOURCE_DIR}"
    VERBATIM)

include("${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/Targets.cmake")

include("${FIREFLY_SOURCE_DIR}/firefly/tests/session-permissions/Targets.cmake")

include("${FIREFLY_SOURCE_DIR}/firefly/tests/frames/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/web-request/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/background-color/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/spellcheck/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/window-constraints/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/window-input/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/tests/title-bar/Targets.cmake")
