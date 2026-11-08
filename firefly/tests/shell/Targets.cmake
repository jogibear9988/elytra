find_package(Threads REQUIRED)
if(APPLE)
    # Exact macOS provider; all desktop singleton methods are intercepted by peers before any
    # launch/trash operation. No original GUI/filesystem peer is called.
    add_executable(firefly-shell-provider-probe "${CMAKE_CURRENT_LIST_DIR}/ProviderProbe.mm")
    target_include_directories(firefly-shell-provider-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-shell-provider-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-shell-provider-probe PRIVATE firefly-shell Threads::Threads)
    add_custom_target(
        check-firefly-shell-provider
        COMMAND "$<TARGET_FILE:firefly-shell-provider-probe>"
        DEPENDS firefly-shell-provider-probe
        VERBATIM)
    # Opt-in only: genuine native trash of one owned fixture, with exact restoration.
    add_executable(firefly-shell-native-trash-probe "${CMAKE_CURRENT_LIST_DIR}/NativeTrashProbe.mm")
    target_include_directories(firefly-shell-native-trash-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-shell-native-trash-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-shell-native-trash-probe PRIVATE firefly-shell Threads::Threads)
    add_custom_target(
        check-firefly-shell-native-trash
        COMMAND "$<TARGET_FILE:firefly-shell-native-trash-probe>"
        DEPENDS firefly-shell-native-trash-probe
        VERBATIM)
endif()

add_executable(firefly-shell-host-probe "${CMAKE_CURRENT_LIST_DIR}/HostProbe.cpp"
                                        "${FIREFLY_SOURCE_DIR}/firefly/native/shell/Host.cpp")
target_include_directories(firefly-shell-host-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-shell-host-probe PRIVATE firefly-mite Qt::Core Threads::Threads)
add_custom_target(
    check-firefly-shell-host
    COMMAND "$<TARGET_FILE:firefly-shell-host-probe>" "${CMAKE_CURRENT_LIST_DIR}/host.mjs"
    DEPENDS firefly-shell-host-probe
    VERBATIM)

# Compile the actual unavailable provider even on macOS. This tests its refusal contract; it does
# not establish native Windows/Linux desktop support.
add_executable(
    firefly-shell-unavailable-probe
    "${CMAKE_CURRENT_LIST_DIR}/UnsupportedProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/shell/ProviderUnsupported.cpp")
target_include_directories(firefly-shell-unavailable-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
add_custom_target(
    check-firefly-shell-unavailable
    COMMAND "$<TARGET_FILE:firefly-shell-unavailable-probe>"
    DEPENDS firefly-shell-unavailable-probe
    VERBATIM)
