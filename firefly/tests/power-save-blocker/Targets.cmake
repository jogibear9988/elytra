if(APPLE)
    # Real ProviderMac implementation, but executable-owned IOPM symbols never invoke the original
    # OS functions or change any real power assertion.
    add_executable(firefly-power-save-blocker-provider-probe
                   "${CMAKE_CURRENT_LIST_DIR}/ProviderProbe.mm")
    target_include_directories(firefly-power-save-blocker-provider-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-power-save-blocker-provider-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-power-save-blocker-provider-probe
                          PRIVATE firefly-power-save-blocker)
    add_custom_target(
        check-firefly-power-save-blocker-provider
        COMMAND "$<TARGET_FILE:firefly-power-save-blocker-provider-probe>"
        DEPENDS firefly-power-save-blocker-provider-probe
        VERBATIM)
endif()

add_executable(
    firefly-power-save-host-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/power-save-blocker/HostProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/power-save-blocker/Host.cpp")
target_include_directories(firefly-power-save-host-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-power-save-host-probe PRIVATE firefly-mite)
add_custom_target(
    check-firefly-power-save-host
    COMMAND "$<TARGET_FILE:firefly-power-save-host-probe>"
            "${FIREFLY_SOURCE_DIR}/firefly/tests/power-save-blocker/host.mjs"
    DEPENDS firefly-power-save-host-probe
    VERBATIM)

if(APPLE)
    add_executable(firefly-power-save-native-probe "${CMAKE_CURRENT_LIST_DIR}/NativeProbe.mm")
    target_include_directories(firefly-power-save-native-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-power-save-native-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-power-save-native-probe PRIVATE firefly-power-save-blocker)
    # Opt-in only: this creates/releases actual transient idle-sleep assertions. It is deliberately
    # not a dependency of the default component/runtime suite.
    add_custom_target(
        check-firefly-power-save-native
        COMMAND "$<TARGET_FILE:firefly-power-save-native-probe>"
        DEPENDS firefly-power-save-native-probe
        VERBATIM)
endif()

# Compile the actual unavailable provider independently on every host, including macOS. This is
# refusal-policy coverage, not acceptance of another OS backend.
add_executable(
    firefly-power-save-unsupported-probe
    "${CMAKE_CURRENT_LIST_DIR}/UnsupportedProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/power-save-blocker/ProviderUnsupported.cpp")
target_include_directories(firefly-power-save-unsupported-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
add_custom_target(
    check-firefly-power-save-unsupported
    COMMAND "$<TARGET_FILE:firefly-power-save-unsupported-probe>"
    DEPENDS firefly-power-save-unsupported-probe
    VERBATIM)
