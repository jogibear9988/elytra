find_package(Threads REQUIRED)
if(APPLE)
    add_executable(firefly-network-state-provider-probe
                   "${CMAKE_CURRENT_LIST_DIR}/NetworkStateProbe.mm")
    target_include_directories(firefly-network-state-provider-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/native/platform")
    target_compile_options(firefly-network-state-provider-probe PRIVATE -fobjc-arc)
    target_link_libraries(firefly-network-state-provider-probe PRIVATE firefly-network-state
                                                                       Threads::Threads)
    add_custom_target(
        check-firefly-network-state-provider
        COMMAND "$<TARGET_FILE:firefly-network-state-provider-probe>"
        DEPENDS firefly-network-state-provider-probe
        VERBATIM)
endif()

add_executable(firefly-net-native-probe "${CMAKE_CURRENT_LIST_DIR}/Native.cpp"
                                        "${FIREFLY_SOURCE_DIR}/firefly/native/net/Host.cpp")
target_include_directories(firefly-net-native-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-net-native-probe PRIVATE firefly-mite firefly-network-state)
add_custom_target(
    check-firefly-net-native
    COMMAND "$<TARGET_FILE:firefly-net-native-probe>" "${CMAKE_CURRENT_LIST_DIR}/native.mjs"
    DEPENDS firefly-net-native-probe
    VERBATIM)

add_executable(
    firefly-network-state-unavailable-probe
    "${CMAKE_CURRENT_LIST_DIR}/Unavailable.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/platform/NetworkStateUnsupported.cpp")
target_include_directories(firefly-network-state-unavailable-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
add_custom_target(
    check-firefly-network-state-unavailable
    COMMAND "$<TARGET_FILE:firefly-network-state-unavailable-probe>"
    DEPENDS firefly-network-state-unavailable-probe
    VERBATIM)
