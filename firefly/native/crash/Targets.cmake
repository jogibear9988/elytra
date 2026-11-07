# Only the renderer/desktop graph owns this policy; standalone Mite is unchanged.
include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
set(crash_output "${CMAKE_BINARY_DIR}/firefly-adapters/crash")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_crash_hooks.py" --output
            "${crash_output}" RESULT_VARIABLE crash_check)
if(NOT crash_check EQUAL 0)
    message(FATAL_ERROR "Review crash-report compositions")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_crash_hooks.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/CrashReport.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/CrashReportStore.cpp")
firefly_replace_source(LibWebView "Libraries/LibWebView/CrashReport.cpp"
                       "${crash_output}/CrashReport.cpp")
firefly_replace_source(LibWebView "Libraries/LibWebView/CrashReportStore.cpp"
                       "${crash_output}/CrashReportStore.cpp")
target_sources(LibWebView PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Reporter.cpp")
add_custom_target(
    check-firefly-crash-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_crash_hooks.py" --output
            "${crash_output}"
    VERBATIM)
add_dependencies(LibWebView check-firefly-crash-hooks)
