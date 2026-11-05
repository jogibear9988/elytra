# Reuse upstream's actual presentation/input widget and Core/Qt event loop.
# Keep a curated list: no browser Application, tabs, chrome or copied sources.
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Widgets GuiPrivate)
set(firefly_qt_sources
    ChromeStyle CrashReportReviewWidget EventLoopImplementationQt
    EventLoopImplementationQtEventTarget Icon InputMethodUtils Popover ReportReview
    ScreenshotSelectionOverlay ScreenshotViewer SelectDropdown StringUtils
    SystemAppearance Theme WebCompatReportReview WebContentView)
set(firefly_qt_files)
foreach(source IN LISTS firefly_qt_sources)
    list(APPEND firefly_qt_files "${LADYBIRD_SOURCE_DIR}/UI/Qt/${source}.cpp")
endforeach()
add_library(firefly-qt-view STATIC "${CMAKE_CURRENT_LIST_DIR}/ContextMenu.cpp" ${firefly_qt_files})
set_target_properties(firefly-qt-view PROPERTIES AUTOMOC ON)
target_include_directories(firefly-qt-view PUBLIC "${LADYBIRD_SOURCE_DIR}" "${LADYBIRD_SOURCE_DIR}/Libraries")
target_link_libraries(firefly-qt-view PUBLIC LibWebView LibIPC LibURL LibGfx LibCore LibMain AK Qt::Core Qt::Gui Qt::Widgets Qt::GuiPrivate)
if(APPLE)
    foreach(source Conversions DictionaryLookup LadybirdAccessibilityElement MacWindow MacWindowFocus
        NativeTextEditingBindings SystemAppearanceMac WebContentViewAccessibility WebContentViewMac)
        target_sources(firefly-qt-view PRIVATE "${LADYBIRD_SOURCE_DIR}/UI/Qt/${source}.mm")
    endforeach()
    set_source_files_properties(
        "${LADYBIRD_SOURCE_DIR}/UI/Qt/LadybirdAccessibilityElement.mm"
        "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentViewAccessibility.mm"
        PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(firefly-qt-view PUBLIC "-framework ApplicationServices" "-framework Cocoa" "-framework IOSurface" "-framework QuartzCore")
endif()
add_executable(firefly-window-probe "${CMAKE_CURRENT_LIST_DIR}/WindowProbe.cpp")
target_link_libraries(firefly-window-probe PRIVATE firefly-qt-view)
target_compile_definitions(firefly-window-probe PRIVATE FIREFLY_LADYBIRD_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
add_dependencies(firefly-window-probe ladybird ${ladybird_helper_processes} check-firefly-upstream)
ladybird_check_skia_confinement(firefly-window-probe)
add_custom_target(check-firefly-window
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/test_window.py"
        --probe "$<TARGET_FILE:firefly-window-probe>"
    DEPENDS firefly-window-probe USES_TERMINAL VERBATIM)

if(FIREFLY_BUILD_RUNTIME)
    include("${FIREFLY_SOURCE_DIR}/mite/cmake/Runtime.cmake")
    mite_add_runtime(firefly-mite "${FIREFLY_SOURCE_DIR}" LibJS)
    add_custom_target(check-firefly-dependencies
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/firefly_dependencies.py"
            --check "${VCPKG_MANIFEST_DIR}" VERBATIM)
    add_dependencies(firefly-mite check-firefly-dependencies)
    file(READ "${FIREFLY_SOURCE_DIR}/firefly/core/firefly.js" FIREFLY_API_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/firefly/core/firefly.js")
    configure_file("${FIREFLY_SOURCE_DIR}/firefly/core/ApiSource.h.in" "${CMAKE_BINARY_DIR}/firefly-generated/ApiSource.h" @ONLY)
    add_executable(firefly "${CMAKE_CURRENT_LIST_DIR}/Application.cpp" "${CMAKE_CURRENT_LIST_DIR}/Bridge.cpp")
    target_include_directories(firefly PRIVATE "${CMAKE_BINARY_DIR}/firefly-generated")
    target_compile_definitions(firefly PRIVATE FIREFLY_LADYBIRD_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
    target_link_libraries(firefly PRIVATE firefly-qt-view firefly-mite)
    add_dependencies(firefly ladybird ${ladybird_helper_processes} check-firefly-upstream)
    ladybird_check_skia_confinement(firefly)
    set(FIREFLY_ELECTRON_REFERENCE "${FIREFLY_SOURCE_DIR}/build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron" CACHE FILEPATH "Electron behavioral reference executable")
    add_custom_target(check-firefly
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/test_firefly.py"
            --firefly "$<TARGET_FILE:firefly>" --electron "${FIREFLY_ELECTRON_REFERENCE}"
        DEPENDS firefly USES_TERMINAL VERBATIM)
endif()
