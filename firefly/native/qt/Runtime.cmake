# Main-process desktop integration still requires the macOS native services.
if(NOT APPLE)
    message(FATAL_ERROR "Linux Qt window probes do not provide the Firefly desktop runtime")
endif()
add_custom_target(
    check-firefly-js
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_firefly_js.py"
    VERBATIM)
include("${FIREFLY_SOURCE_DIR}/mite/cmake/Runtime.cmake")
mite_add_runtime(firefly-mite "${FIREFLY_SOURCE_DIR}" LibJS)
add_custom_target(
    check-firefly-dependencies
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/firefly_dependencies.py" --check
            "${VCPKG_MANIFEST_DIR}"
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
add_executable(
    firefly
    "${CMAKE_CURRENT_LIST_DIR}/Application.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Bridge.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../clipboard/ClipboardHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../clipboard/ClipboardMac.mm"
    "${CMAKE_CURRENT_LIST_DIR}/../theme/ThemeHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../theme/ThemeMac.mm"
    "${CMAKE_CURRENT_LIST_DIR}/../window/Geometry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../window/GeometryMac.mm"
    "${CMAKE_CURRENT_LIST_DIR}/../window/FrameGeometry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenMac.mm"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeData.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeWire.cpp")
target_include_directories(firefly PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge"
                                           "${FIREFLY_SOURCE_DIR}/firefly/native/ipc")
target_include_directories(firefly PRIVATE "${CMAKE_BINARY_DIR}/firefly-generated")
target_compile_definitions(
    firefly
    PRIVATE FIREFLY_LADYBIRD_DIRECTORY="${CMAKE_BINARY_DIR}/FireflyRuntimeHelper.app/Contents/MacOS"
)
target_link_libraries(firefly PRIVATE firefly-qt-view firefly-mite firefly-clipboard-protocol)
add_dependencies(firefly ladybird ${ladybird_helper_processes} check-firefly-upstream)
ladybird_check_skia_confinement(firefly)
set(FIREFLY_ELECTRON_REFERENCE
    "${FIREFLY_SOURCE_DIR}/build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron"
    CACHE FILEPATH "Electron behavioral reference executable")
add_custom_target(
    check-firefly
    COMMAND "$<TARGET_FILE:firefly-clipboard-protocol-probe>"
    COMMAND "$<TARGET_FILE:firefly-chrome-probe>"
    COMMAND "$<TARGET_FILE:firefly-screen-probe>"
    COMMAND "$<TARGET_FILE:firefly-clipboard-probe>"
    COMMAND "$<TARGET_FILE:firefly-theme-probe>"
    COMMAND "$<TARGET_FILE:firefly-geometry-probe>"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_firefly.py" --firefly
            "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_public_preload.py"
            --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_renderer_modules.py"
            --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_public_ipc.py"
            --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
    DEPENDS firefly
            firefly-chrome-probe
            firefly-screen-probe
            firefly-clipboard-probe
            firefly-clipboard-protocol-probe
            firefly-theme-probe
            firefly-geometry-probe
    USES_TERMINAL VERBATIM)
