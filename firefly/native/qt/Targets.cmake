# Reuse upstream's actual presentation/input widget and Core/Qt event loop. Keep a curated list: no
# browser Application, tabs, chrome or copied sources.
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Widgets GuiPrivate)
include("${FIREFLY_SOURCE_DIR}/firefly/native/ProcessMonitor.cmake")
set(firefly_qt_sources
    ChromeStyle
    CrashReportReviewWidget
    EventLoopImplementationQt
    EventLoopImplementationQtEventTarget
    Icon
    InputMethodUtils
    Popover
    ReportReview
    ScreenshotSelectionOverlay
    ScreenshotViewer
    SelectDropdown
    StringUtils
    SystemAppearance
    Theme
    WebCompatReportReview
    WebContentView)
set(firefly_qt_files)
foreach(source IN LISTS firefly_qt_sources)
    if(source STREQUAL "WebContentView")
        set(prepared_view "${CMAKE_BINARY_DIR}/firefly-generated/WebContentView.cpp")
        execute_process(
            COMMAND
                "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_preload_hook.py"
                --output "${prepared_view}"
            RESULT_VARIABLE preparation_check)
        if(NOT preparation_check EQUAL 0)
            message(FATAL_ERROR "Review owned Qt preload preparation adapter")
        endif()
        set_property(
            DIRECTORY
            APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS
                     "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_preload_hook.py"
                     "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentView.cpp"
                     "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentView.h")
        list(APPEND firefly_qt_files "${prepared_view}")
    else()
        list(APPEND firefly_qt_files "${LADYBIRD_SOURCE_DIR}/UI/Qt/${source}.cpp")
    endif()
endforeach()
add_library(
    firefly-qt-view STATIC
    "${CMAKE_CURRENT_LIST_DIR}/PreloadHooks.cpp" "${CMAKE_CURRENT_LIST_DIR}/ContextMenu.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ChromeWindow.cpp" "${CMAKE_CURRENT_LIST_DIR}/ToolTip.cpp"
    ${firefly_qt_files})
set_target_properties(firefly-qt-view PROPERTIES AUTOMOC ON)
target_include_directories(firefly-qt-view PUBLIC "${LADYBIRD_SOURCE_DIR}"
                                                  "${LADYBIRD_SOURCE_DIR}/Libraries")
target_include_directories(firefly-qt-view PRIVATE "${CMAKE_CURRENT_LIST_DIR}"
                                                   "${LADYBIRD_SOURCE_DIR}/UI/Qt")
target_sources(firefly-qt-view PRIVATE "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentView.h")
add_custom_target(
    check-firefly-preload-view
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_preload_hook.py"
            --output "${prepared_view}"
    VERBATIM)
add_dependencies(firefly-qt-view check-firefly-preload-view)
target_link_libraries(
    firefly-qt-view
    PUBLIC LibWebView
           LibIPC
           LibURL
           LibGfx
           LibCore
           LibMain
           AK
           Qt::Core
           Qt::Gui
           Qt::Widgets
           Qt::GuiPrivate)
if(APPLE)
    foreach(
        source
        Conversions
        DictionaryLookup
        LadybirdAccessibilityElement
        MacWindow
        MacWindowFocus
        NativeTextEditingBindings
        SystemAppearanceMac
        WebContentViewAccessibility
        WebContentViewMac)
        target_sources(firefly-qt-view PRIVATE "${LADYBIRD_SOURCE_DIR}/UI/Qt/${source}.mm")
    endforeach()
    set_source_files_properties(
        "${LADYBIRD_SOURCE_DIR}/UI/Qt/LadybirdAccessibilityElement.mm"
        "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentViewAccessibility.mm" PROPERTIES COMPILE_FLAGS
                                                                                 "-fobjc-arc")
    target_link_libraries(
        firefly-qt-view PUBLIC "-framework ApplicationServices" "-framework Cocoa"
                               "-framework IOSurface" "-framework QuartzCore")
endif()
add_executable(firefly-window-probe "${CMAKE_CURRENT_LIST_DIR}/WindowProbe.cpp")
target_link_libraries(firefly-window-probe PRIVATE firefly-qt-view)
target_compile_definitions(firefly-window-probe
                           PRIVATE FIREFLY_LADYBIRD_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
add_dependencies(firefly-window-probe ladybird ${ladybird_helper_processes} check-firefly-upstream)
ladybird_check_skia_confinement(firefly-window-probe)
add_custom_target(
    check-firefly-window
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_window.py" --probe
            "$<TARGET_FILE:firefly-window-probe>"
    DEPENDS firefly-window-probe
    USES_TERMINAL VERBATIM)

add_executable(
    firefly-chrome-probe
    "${CMAKE_CURRENT_LIST_DIR}/ChromeProbe.cpp" "${CMAKE_CURRENT_LIST_DIR}/ChromeWindow.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ToolTip.cpp")
target_link_libraries(firefly-chrome-probe PRIVATE Qt::Widgets)

include("${FIREFLY_SOURCE_DIR}/firefly/native/ipc/Targets.cmake")

add_executable(firefly-clipboard-probe "${CMAKE_CURRENT_LIST_DIR}/../clipboard/Probe.mm"
                                       "${CMAKE_CURRENT_LIST_DIR}/../clipboard/ClipboardMac.mm")
target_link_libraries(firefly-clipboard-probe PRIVATE Qt::Core "-framework AppKit")

add_executable(firefly-screen-probe "${CMAKE_CURRENT_LIST_DIR}/../screen/Probe.cpp"
                                    "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenMac.mm")
target_link_libraries(firefly-screen-probe PRIVATE Qt::Core "-framework AppKit"
                                                   "-framework CoreGraphics")

if(FIREFLY_BUILD_RUNTIME)
    add_custom_target(
        check-firefly-js
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_firefly_js.py"
        VERBATIM)
    include("${FIREFLY_SOURCE_DIR}/mite/cmake/Runtime.cmake")
    mite_add_runtime(firefly-mite "${FIREFLY_SOURCE_DIR}" LibJS)
    add_custom_target(
        check-firefly-dependencies
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/firefly_dependencies.py"
                --check "${VCPKG_MANIFEST_DIR}"
        VERBATIM)
    add_dependencies(firefly-mite check-firefly-dependencies)
    file(READ "${FIREFLY_SOURCE_DIR}/firefly/core/generated/api.js" FIREFLY_API_SOURCE)
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/firefly/core/generated/api.js")
    configure_file("${FIREFLY_SOURCE_DIR}/firefly/core/ApiSource.h.in"
                   "${CMAKE_BINARY_DIR}/firefly-generated/ApiSource.h" @ONLY)
    file(READ "${FIREFLY_SOURCE_DIR}/firefly/core/generated/app-regions.js"
         FIREFLY_APP_REGIONS_SOURCE)
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${FIREFLY_SOURCE_DIR}/firefly/core/generated/app-regions.js")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/AppRegions.h.in"
                   "${CMAKE_BINARY_DIR}/firefly-generated/AppRegions.h" @ONLY)
    add_executable(
        firefly
        "${CMAKE_CURRENT_LIST_DIR}/Application.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Bridge.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../clipboard/ClipboardHost.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../clipboard/ClipboardMac.mm"
        "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenHost.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../screen/ScreenMac.mm"
        "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeData.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeWire.cpp")
    target_include_directories(firefly PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge"
                                               "${FIREFLY_SOURCE_DIR}/firefly/native/ipc")
    target_include_directories(firefly PRIVATE "${CMAKE_BINARY_DIR}/firefly-generated")
    target_compile_definitions(
        firefly
        PRIVATE
            FIREFLY_LADYBIRD_DIRECTORY="${CMAKE_BINARY_DIR}/FireflyRuntimeHelper.app/Contents/MacOS"
    )
    target_link_libraries(firefly PRIVATE firefly-qt-view firefly-mite)
    add_dependencies(firefly ladybird ${ladybird_helper_processes} check-firefly-upstream)
    ladybird_check_skia_confinement(firefly)
    set(FIREFLY_ELECTRON_REFERENCE
        "${FIREFLY_SOURCE_DIR}/build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron"
        CACHE FILEPATH "Electron behavioral reference executable")
    add_custom_target(
        check-firefly
        COMMAND "$<TARGET_FILE:firefly-chrome-probe>"
        COMMAND "$<TARGET_FILE:firefly-screen-probe>"
        COMMAND "$<TARGET_FILE:firefly-clipboard-probe>"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_firefly.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_public_preload.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_renderer_modules.py"
            --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_public_ipc.py"
                --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        DEPENDS firefly firefly-chrome-probe firefly-screen-probe firefly-clipboard-probe
        USES_TERMINAL VERBATIM)
endif()
