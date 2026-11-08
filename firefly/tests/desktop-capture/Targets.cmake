# Compile the exact production dispatcher against a controlled native completion peer. No OS
# provider, Qt GUI plugin, capture entitlement or TCC interaction is involved.
find_package(Threads REQUIRED)
add_executable(
    firefly-desktop-capture-host-probe
    "${CMAKE_CURRENT_LIST_DIR}/HostProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/desktop-capture/Host.cpp")
target_include_directories(firefly-desktop-capture-host-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-desktop-capture-host-probe PRIVATE firefly-mite Qt::Core
                                                                 Threads::Threads)
add_custom_target(
    check-firefly-desktop-capture-host
    COMMAND "$<TARGET_FILE:firefly-desktop-capture-host-probe>"
    DEPENDS firefly-desktop-capture-host-probe
    VERBATIM)
