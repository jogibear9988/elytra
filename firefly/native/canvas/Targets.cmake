# An unregistered upstream view keeps its original system canvas selection.
include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
set(canvas_ui_output "${CMAKE_BINARY_DIR}/firefly-adapters/canvas-ui")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_preferences.py"
            --output-dir "${canvas_ui_output}" RESULT_VARIABLE canvas_ui_check)
if(NOT canvas_ui_check EQUAL 0)
    message(FATAL_ERROR "Review native canvas UI source adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/prepare_canvas_ui.py"
             "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_preferences.py"
             "${FIREFLY_SOURCE_DIR}/firefly/native/zoom/View.h"
             "${FIREFLY_SOURCE_DIR}/firefly/native/zoom/View.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/ViewImplementation.cpp")
firefly_replace_source(LibWebView "Libraries/LibWebView/ViewImplementation.cpp"
                       "${canvas_ui_output}/ViewImplementation.cpp")
target_sources(LibWebView PRIVATE "${CMAKE_CURRENT_LIST_DIR}/UI.cpp"
                                  "${FIREFLY_SOURCE_DIR}/firefly/native/zoom/View.cpp")
target_include_directories(LibWebView PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
add_custom_target(
    check-firefly-canvas-ui
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_preferences.py"
            --output-dir "${canvas_ui_output}"
    VERBATIM)
add_dependencies(LibWebView check-firefly-canvas-ui)
