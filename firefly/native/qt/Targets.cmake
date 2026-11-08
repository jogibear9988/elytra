# Reuse upstream's actual presentation/input widget and Core/Qt event loop. Keep a curated list: no
# browser Application, tabs, chrome or copied sources.
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Widgets GuiPrivate)
include("${FIREFLY_SOURCE_DIR}/firefly/native/clipboard/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/window/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/screen/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/desktop-capture/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/notifications/Targets.cmake")
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
    include("${CMAKE_CURRENT_LIST_DIR}/Mac.cmake")
elseif(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/Windows.cmake")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_LIST_DIR}/Linux.cmake")
else()
    message(FATAL_ERROR "No native Firefly Qt presentation adapter for this platform")
endif()
add_library(firefly-file-picker STATIC "${CMAKE_CURRENT_LIST_DIR}/../file-picker/Picker.cpp"
                                       "${CMAKE_CURRENT_LIST_DIR}/../file-picker/Snapshot.cpp")
target_link_libraries(firefly-file-picker PUBLIC Qt::Widgets LibWebCommon LibCore LibIPC AK)
add_executable(firefly-file-picker-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/file-picker/NativeProbe.cpp")
target_include_directories(firefly-file-picker-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-file-picker-probe PRIVATE firefly-file-picker)
add_custom_target(
    check-firefly-file-picker
    COMMAND "$<TARGET_FILE:firefly-file-picker-probe>"
    DEPENDS firefly-file-picker-probe
    VERBATIM)
add_executable(firefly-file-input-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/file-picker/ViewProbe.cpp")
target_include_directories(firefly-file-input-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-file-input-probe PRIVATE firefly-file-picker firefly-qt-view)
target_compile_definitions(firefly-file-input-probe
                           PRIVATE FIREFLY_LADYBIRD_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
add_dependencies(firefly-file-input-probe ladybird ${ladybird_helper_processes}
                 check-firefly-upstream)
ladybird_check_skia_confinement(firefly-file-input-probe)
add_custom_target(
    check-firefly-file-input
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_file_picker.py"
            --probe "$<TARGET_FILE:firefly-file-input-probe>"
    DEPENDS firefly-file-input-probe
    VERBATIM)
add_executable(firefly-window-probe "${CMAKE_CURRENT_LIST_DIR}/WindowProbe.cpp")
target_link_libraries(firefly-window-probe PRIVATE firefly-qt-view)
target_compile_definitions(firefly-window-probe
                           PRIVATE FIREFLY_LADYBIRD_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
add_dependencies(firefly-window-probe ladybird ${ladybird_helper_processes} check-firefly-upstream)
ladybird_check_skia_confinement(firefly-window-probe)
if(WIN32)
    ladybird_windows_bin(firefly-window-probe CONSOLE)
    # The upstream ladybird dependency deploys native Qt platform plugins into this same SDK bin
    # directory; keep its native layout instead of copying Qt.
endif()
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

if(APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/MacProbes.cmake")
endif()

if(FIREFLY_BUILD_RUNTIME)
    include("${CMAKE_CURRENT_LIST_DIR}/Runtime.cmake")
endif()
