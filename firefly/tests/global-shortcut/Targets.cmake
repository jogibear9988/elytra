# These peers never register OS hotkeys or synthesize keyboard input.
find_package(Threads REQUIRED)
add_executable(
    firefly-global-shortcut-host-probe
    "${CMAKE_CURRENT_LIST_DIR}/HostProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/global-shortcut/Host.cpp")
target_include_directories(firefly-global-shortcut-host-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-global-shortcut-host-probe PRIVATE firefly-mite Qt::Core Qt::Gui
                                                                 Threads::Threads)
add_custom_target(
    check-firefly-global-shortcut-host
    COMMAND "$<TARGET_FILE:firefly-global-shortcut-host-probe>" "${CMAKE_CURRENT_LIST_DIR}/host.mjs"
    COMMAND "$<TARGET_FILE:firefly-global-shortcut-host-probe>" "${CMAKE_CURRENT_LIST_DIR}/host.mjs"
            --leave-active
    DEPENDS firefly-global-shortcut-host-probe
    VERBATIM)

if(APPLE)
    add_executable(firefly-global-shortcut-provider-probe
                   "${CMAKE_CURRENT_LIST_DIR}/ProviderProbe.mm")
    target_include_directories(firefly-global-shortcut-provider-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-global-shortcut-provider-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-global-shortcut-provider-probe PRIVATE firefly-global-shortcut)
    add_custom_target(
        check-firefly-global-shortcut-provider
        COMMAND "$<TARGET_FILE:firefly-global-shortcut-provider-probe>"
        DEPENDS firefly-global-shortcut-provider-probe
        VERBATIM)
endif()

add_executable(
    firefly-global-shortcut-unavailable-probe
    "${CMAKE_CURRENT_LIST_DIR}/UnsupportedProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/global-shortcut/ProviderUnsupported.cpp")
target_include_directories(firefly-global-shortcut-unavailable-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
add_custom_target(
    check-firefly-global-shortcut-unavailable
    COMMAND "$<TARGET_FILE:firefly-global-shortcut-unavailable-probe>"
    DEPENDS firefly-global-shortcut-unavailable-probe
    VERBATIM)
if(APPLE)
    add_executable(firefly-global-shortcut-native-probe "${CMAKE_CURRENT_LIST_DIR}/NativeProbe.mm")
    target_include_directories(firefly-global-shortcut-native-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-global-shortcut-native-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-global-shortcut-native-probe PRIVATE firefly-global-shortcut)
    # Explicit opt-in: no ordinary build/check registers system-wide keys.
    add_custom_target(
        check-firefly-global-shortcut-native
        COMMAND "$<TARGET_FILE:firefly-global-shortcut-native-probe>"
        DEPENDS firefly-global-shortcut-native-probe
        VERBATIM)
endif()
