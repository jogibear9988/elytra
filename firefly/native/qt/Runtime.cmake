# Select real native adapters; the Linux runtime remains experimental until its hosted gate passes.
if(NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "No native Firefly desktop runtime for this platform")
endif()
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-helper/Layout.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../platform/RuntimePaths.cmake")
add_custom_target(
    check-firefly-js
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_firefly_js.py"
    VERBATIM)
include("${FIREFLY_SOURCE_DIR}/mite/cmake/Runtime.cmake")
mite_add_runtime(firefly-mite "${FIREFLY_SOURCE_DIR}" LibJS)
add_custom_target(
    check-firefly-dependencies
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/firefly_dependencies.py" --check
            "${VCPKG_MANIFEST_DIR}" ${firefly_runtime_dependency_args}
    VERBATIM)
add_dependencies(firefly-mite check-firefly-dependencies)
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
        "${CMAKE_CURRENT_LIST_DIR}/../window/GeometryMac.mm")
else()
    list(APPEND firefly_native_services "${CMAKE_CURRENT_LIST_DIR}/../window/GeometryLinux.cpp")
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
add_executable(
    firefly
    "${CMAKE_CURRENT_LIST_DIR}/Application.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Bridge.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Lifecycle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Ipc.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Preload.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Windows.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bridge/Host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../clipboard/ClipboardHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../window/Geometry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../window/FrameGeometry.cpp"
    ${firefly_native_services}
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeData.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeWire.cpp")
target_include_directories(firefly PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge"
                                           "${FIREFLY_SOURCE_DIR}/firefly/native/ipc")
target_include_directories(firefly PRIVATE "${CMAKE_BINARY_DIR}/firefly-generated")
target_compile_definitions(firefly
                           PRIVATE FIREFLY_LADYBIRD_DIRECTORY="${firefly_runtime_binary_dir}")
target_link_libraries(firefly PRIVATE firefly-qt-view firefly-mite firefly-clipboard-protocol
                                      firefly-runtime-paths firefly-screen-events)
if(NOT APPLE)
    target_link_libraries(firefly PRIVATE firefly-linux-desktop-services firefly-x11-clipboard)
endif()
add_dependencies(firefly ladybird ${ladybird_helper_processes} check-firefly-upstream)
ladybird_check_skia_confinement(firefly)
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
        DEPENDS firefly
                check-firefly-protocol-origins
                check-firefly-protocol-files
                check-firefly-protocol-fetch
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
