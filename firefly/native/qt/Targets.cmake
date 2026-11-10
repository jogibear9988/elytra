# Reuse upstream's actual presentation/input widget and Core/Qt event loop. Keep a curated list: no
# browser Application, tabs, chrome or copied sources.
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Widgets GuiPrivate)
include("${FIREFLY_SOURCE_DIR}/firefly/native/locale/Targets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/hub/Targets.cmake")
set(hub_frontend_output "${CMAKE_BINARY_DIR}/firefly-adapters/hub-frontend")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_hub_frontend.py" --source
            "${LADYBIRD_SOURCE_DIR}" --output "${hub_frontend_output}" --self-test
    RESULT_VARIABLE hub_frontend_result)
if(NOT hub_frontend_result EQUAL 0)
    message(FATAL_ERROR "Review embedded Hub frontend source composition")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/prepare_hub_frontend.py"
             "${LADYBIRD_SOURCE_DIR}/UI/Qt/RemoteView.h"
             "${LADYBIRD_SOURCE_DIR}/UI/Qt/RemoteView.cpp"
             "${LADYBIRD_SOURCE_DIR}/UI/Qt/HubConnection.cpp"
             "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentView.cpp"
             "${LADYBIRD_SOURCE_DIR}/UI/Qt/ContextMenus.cpp")
set(prepared_view "${CMAKE_BINARY_DIR}/firefly-generated/HubView.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_preload_hook.py"
            --output "${prepared_view}" RESULT_VARIABLE preparation_check)
if(NOT preparation_check EQUAL 0)
    message(FATAL_ERROR "Review canonical Hub preload preparation adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_preload_hook.py"
             "${LADYBIRD_SOURCE_DIR}/Services/Hub/View.cpp"
             "${LADYBIRD_SOURCE_DIR}/Services/Hub/View.h")
firefly_replace_source(hubservice "Services/Hub/View.cpp" "${prepared_view}")
target_sources(hubservice PRIVATE "${CMAKE_CURRENT_LIST_DIR}/PreloadHooks.cpp")
target_include_directories(hubservice PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
include("${FIREFLY_SOURCE_DIR}/firefly/native/clipboard/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/window/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/screen/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/desktop-capture/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/notifications/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/power-monitor/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/power-save-blocker/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/shell/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/system-preferences/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/net/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/global-shortcut/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/dialog/Targets.cmake")
add_library(firefly-gpu-policy STATIC "${FIREFLY_SOURCE_DIR}/firefly/native/gpu/State.cpp")
include("${FIREFLY_SOURCE_DIR}/firefly/native/ProcessMonitor.cmake")
set(firefly_qt_sources
    Actions
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
    WebContentView
    RemoteView
    HubConnection
    ContextMenus)
set(firefly_qt_files)
foreach(source IN LISTS firefly_qt_sources)
    if(source MATCHES "^(WebContentView|RemoteView|HubConnection|ContextMenus)$")
        list(APPEND firefly_qt_files "${hub_frontend_output}/UI/Qt/${source}.cpp")
    else()
        list(APPEND firefly_qt_files "${LADYBIRD_SOURCE_DIR}/UI/Qt/${source}.cpp")
    endif()
endforeach()
add_library(
    firefly-qt-view STATIC
    "${CMAKE_CURRENT_LIST_DIR}/hub/Host.cpp" "${CMAKE_CURRENT_LIST_DIR}/hub/Frontend.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ContextMenu.cpp" "${CMAKE_CURRENT_LIST_DIR}/ChromeWindow.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ToolTip.cpp" ${firefly_qt_files})
set_target_properties(firefly-qt-view PROPERTIES AUTOMOC ON)
target_include_directories(firefly-qt-view BEFORE PUBLIC "${hub_frontend_output}")
include("${CMAKE_CURRENT_LIST_DIR}/hub/HeaderPolicy.cmake")
target_include_directories(
    firefly-qt-view PUBLIC "${LADYBIRD_SOURCE_DIR}" "${LADYBIRD_SOURCE_DIR}/Libraries"
                           "${LADYBIRD_SOURCE_DIR}/Services" "${CMAKE_BINARY_DIR}/Services")
target_include_directories(
    firefly-qt-view PRIVATE "${FIREFLY_SOURCE_DIR}/firefly" "${CMAKE_CURRENT_LIST_DIR}"
                            "${LADYBIRD_SOURCE_DIR}/UI/Qt")
target_sources(firefly-qt-view PRIVATE "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentView.h")
add_custom_target(
    check-firefly-preload-view
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_preload_hook.py"
            --output "${prepared_view}"
    VERBATIM)
add_dependencies(hubservice check-firefly-preload-view)
add_custom_target(
    check-firefly-hub-frontend
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_hub_frontend.py" --source
            "${LADYBIRD_SOURCE_DIR}" --output "${hub_frontend_output}" --self-test
    VERBATIM)
add_dependencies(firefly-qt-view check-firefly-hub-frontend)
target_link_libraries(
    firefly-qt-view
    # Qt's static ICU archives must not be selected ahead of the shared native Unicode
    # implementation, which maps the actual external data before main.
    PUBLIC LibUnicode
           firefly-locale
           LibWebView
           hubservice
           hubipc
           LibGamepad
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
target_link_libraries(firefly-file-picker PUBLIC LibUnicode Qt::Widgets LibWebCommon LibCore LibIPC
                                                 AK)
get_target_property(firefly_unicode_library_type LibUnicode TYPE)
if(APPLE
   AND firefly_unicode_library_type STREQUAL "SHARED_LIBRARY"
   AND LADYBIRD_ICU_DATA_FILE)
    # Final dependency ordering can still place Qt's static ICU archives before LibUnicode. Present
    # the real external-data owner before archive selection; keep the ordinary target
    # dependency/rpath as well. No second ICU loader.
    target_link_options(firefly-file-picker INTERFACE
                        "LINKER:-needed_library,$<TARGET_FILE:LibUnicode>")
endif()

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

# The two entry graphs previously chose different ICU copies. These native Core processes exercise
# the actual Qt collator without a GUI, dialogs or user locale changes. Keep both graph shapes
# covered instead of manually initializing data.
add_executable(firefly-qt-icu-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/icu/NativeProbe.cpp")
target_link_libraries(firefly-qt-icu-probe PRIVATE firefly-qt-view)
add_executable(firefly-picker-icu-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/icu/NativeProbe.cpp")
target_link_libraries(firefly-picker-icu-probe PRIVATE firefly-file-picker)
add_custom_target(
    check-firefly-qt-icu
    COMMAND "$<TARGET_FILE:firefly-qt-icu-probe>"
    COMMAND "$<TARGET_FILE:firefly-picker-icu-probe>"
    DEPENDS firefly-qt-icu-probe firefly-picker-icu-probe
    VERBATIM)

if(TARGET Qt6::QOffscreenIntegrationPlugin)
    add_executable(
        firefly-hub-lifetime-probe
        "${FIREFLY_SOURCE_DIR}/firefly/tests/hub/LifetimeProbe.cpp"
        "${FIREFLY_SOURCE_DIR}/firefly/tests/web-contents-view/OffscreenPlugin.cpp")
    target_include_directories(firefly-hub-lifetime-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_definitions(firefly-hub-lifetime-probe
                               PRIVATE FIREFLY_LADYBIRD_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
    target_link_libraries(firefly-hub-lifetime-probe PRIVATE firefly-qt-view
                                                             Qt6::QOffscreenIntegrationPlugin)
    add_dependencies(firefly-hub-lifetime-probe ladybird ${ladybird_helper_processes}
                     check-firefly-upstream)
    add_custom_target(
        check-firefly-hub-lifetime
        COMMAND "$<TARGET_FILE:firefly-hub-lifetime-probe>"
        COMMAND "$<TARGET_FILE:firefly-hub-lifetime-probe>" --replacement
        DEPENDS firefly-hub-lifetime-probe
        VERBATIM)
endif()
