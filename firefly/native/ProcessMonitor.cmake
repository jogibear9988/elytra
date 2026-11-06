if(NOT WIN32)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_process_monitor.py"
        RESULT_VARIABLE monitor_check)
    if(NOT monitor_check EQUAL 0)
        message(FATAL_ERROR "Review the owned selective process-monitor adapter")
    endif()
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/check_process_monitor.py"
                 "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/ProcessMonitor.cpp"
                 "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/ProcessMonitor.h")
    get_target_property(sources LibWebView SOURCES)
    get_target_property(source_dir LibWebView SOURCE_DIR)
    get_target_property(binary_dir LibWebView BINARY_DIR)
    set(adapted)
    set(count 0)
    foreach(source IN LISTS sources)
        if(source STREQUAL "ProcessMonitor.cpp" OR source STREQUAL
                                                   "${source_dir}/ProcessMonitor.cpp")
            list(APPEND adapted "${FIREFLY_SOURCE_DIR}/firefly/native/ProcessMonitor.cpp")
            math(EXPR count "${count}+1")
        elseif(IS_ABSOLUTE "${source}" OR source MATCHES "^\\$<")
            list(APPEND adapted "${source}")
        elseif(EXISTS "${source_dir}/${source}")
            list(APPEND adapted "${source_dir}/${source}")
        else()
            list(APPEND adapted "${binary_dir}/${source}")
        endif()
    endforeach()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Expected exactly one upstream process-monitor translation unit")
    endif()
    set_property(TARGET LibWebView PROPERTY SOURCES "${adapted}")
    add_custom_target(
        check-process-monitor
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_process_monitor.py"
        VERBATIM)
    add_dependencies(LibWebView check-process-monitor)
endif()
