# Exact production host with a controlled native completion peer; never links the OS provider.
find_package(Threads REQUIRED)
add_executable(
    firefly-notification-host-probe "${CMAKE_CURRENT_LIST_DIR}/HostProbe.cpp"
                                    "${FIREFLY_SOURCE_DIR}/firefly/native/notifications/Host.cpp")
target_include_directories(firefly-notification-host-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-notification-host-probe PRIVATE firefly-mite Qt::Core
                                                              Threads::Threads)
add_custom_target(
    check-firefly-notification-host
    COMMAND "$<TARGET_FILE:firefly-notification-host-probe>"
    DEPENDS firefly-notification-host-probe
    VERBATIM)

if(APPLE)
    # Actual ProviderMac.mm state machine, with both OS singleton factories fully intercepted before
    # any provider call. No native OS-center IMP is invoked.
    add_executable(firefly-notification-provider-state-probe
                   "${CMAKE_CURRENT_LIST_DIR}/ProviderStateProbe.mm")
    target_include_directories(firefly-notification-provider-state-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-notification-provider-state-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-notification-provider-state-probe PRIVATE firefly-notifications)
    add_custom_target(
        check-firefly-notification-provider-state
        COMMAND "$<TARGET_FILE:firefly-notification-provider-state-probe>"
        DEPENDS firefly-notification-provider-state-probe
        VERBATIM)
endif()
