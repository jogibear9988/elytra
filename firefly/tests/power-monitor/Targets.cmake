# Exact production dispatcher with controlled native queries and completion peers. No OS power
# provider, Qt GUI plugin, lock/sleep/battery transition or assertion is used.
find_package(Threads REQUIRED)
add_executable(
    firefly-power-monitor-host-probe "${CMAKE_CURRENT_LIST_DIR}/HostProbe.cpp"
                                     "${FIREFLY_SOURCE_DIR}/firefly/native/power-monitor/Host.cpp")
target_include_directories(firefly-power-monitor-host-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-power-monitor-host-probe PRIVATE firefly-mite Qt::Core
                                                               Threads::Threads)
add_custom_target(
    check-firefly-power-monitor-host
    COMMAND "$<TARGET_FILE:firefly-power-monitor-host-probe>"
    DEPENDS firefly-power-monitor-host-probe
    VERBATIM)

add_executable(
    firefly-power-monitor-public-probe
    "${CMAKE_CURRENT_LIST_DIR}/PublicProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/power-monitor/Host.cpp")
target_include_directories(firefly-power-monitor-public-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-power-monitor-public-probe PRIVATE firefly-mite firefly-power-monitor
                                                                 Qt::Core)
if(APPLE)
    add_custom_target(
        check-firefly-power-monitor-public
        COMMAND "$<TARGET_FILE:firefly-power-monitor-public-probe>"
                "${CMAKE_CURRENT_LIST_DIR}/native-queries.mjs"
        DEPENDS firefly-power-monitor-public-probe
        VERBATIM)
endif()
