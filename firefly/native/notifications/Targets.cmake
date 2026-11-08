if(APPLE)
    add_library(firefly-notifications STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderMac.mm")
    target_compile_options(firefly-notifications PRIVATE -fobjc-arc)
    target_link_libraries(firefly-notifications PUBLIC "-framework Foundation"
                                                       "-framework UserNotifications")
else()
    add_library(firefly-notifications STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderUnsupported.cpp")
endif()
add_executable(firefly-notification-provider-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/notifications/ProviderProbe.cpp")
target_include_directories(firefly-notification-provider-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-notification-provider-probe PRIVATE firefly-notifications)
add_custom_target(
    check-firefly-notification-provider
    COMMAND "$<TARGET_FILE:firefly-notification-provider-probe>"
    DEPENDS firefly-notification-provider-probe
    VERBATIM)
