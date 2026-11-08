find_package(Threads REQUIRED)
if(APPLE)
    add_executable(firefly-system-preferences-provider-probe
                   "${CMAKE_CURRENT_LIST_DIR}/ProviderProbe.mm")
    target_include_directories(firefly-system-preferences-provider-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-system-preferences-provider-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-system-preferences-provider-probe
                          PRIVATE firefly-system-preferences Threads::Threads)
    add_custom_target(
        check-firefly-system-preferences-provider
        COMMAND "$<TARGET_FILE:firefly-system-preferences-provider-probe>"
        DEPENDS firefly-system-preferences-provider-probe
        VERBATIM)
    add_executable(firefly-system-preferences-read-only-probe
                   "${CMAKE_CURRENT_LIST_DIR}/ReadOnlyProbe.mm")
    target_include_directories(firefly-system-preferences-read-only-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-system-preferences-read-only-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-system-preferences-read-only-probe
                          PRIVATE firefly-system-preferences)
    add_custom_target(
        check-firefly-system-preferences-read-only
        COMMAND "$<TARGET_FILE:firefly-system-preferences-read-only-probe>"
        DEPENDS firefly-system-preferences-read-only-probe
        VERBATIM)
    # Opt-in only: actual writes restricted to a unique owned defaults suite, and notifications
    # posted solely on the process-local center.
    add_executable(firefly-system-preferences-isolated-probe
                   "${CMAKE_CURRENT_LIST_DIR}/IsolatedProbe.mm")
    target_include_directories(firefly-system-preferences-isolated-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_compile_options(firefly-system-preferences-isolated-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-system-preferences-isolated-probe
                          PRIVATE firefly-system-preferences)
    add_custom_target(
        check-firefly-system-preferences-isolated
        COMMAND "$<TARGET_FILE:firefly-system-preferences-isolated-probe>"
        DEPENDS firefly-system-preferences-isolated-probe
        VERBATIM)
endif()
add_executable(
    firefly-system-preferences-host-probe
    "${CMAKE_CURRENT_LIST_DIR}/HostProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/system-preferences/Host.cpp")
target_include_directories(firefly-system-preferences-host-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-system-preferences-host-probe PRIVATE firefly-mite Qt::Core
                                                                    Threads::Threads)
add_custom_target(
    check-firefly-system-preferences-host
    COMMAND "$<TARGET_FILE:firefly-system-preferences-host-probe>"
            "${CMAKE_CURRENT_LIST_DIR}/host.mjs"
    DEPENDS firefly-system-preferences-host-probe
    VERBATIM)

# Always compile the unavailable backend too: refusal is not platform acceptance.
add_executable(
    firefly-system-preferences-unavailable-probe
    "${CMAKE_CURRENT_LIST_DIR}/UnsupportedProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/system-preferences/ProviderUnsupported.cpp")
target_include_directories(firefly-system-preferences-unavailable-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
add_custom_target(
    check-firefly-system-preferences-unavailable
    COMMAND "$<TARGET_FILE:firefly-system-preferences-unavailable-probe>"
    DEPENDS firefly-system-preferences-unavailable-probe
    VERBATIM)
