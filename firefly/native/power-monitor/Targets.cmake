if(APPLE)
    add_library(firefly-power-monitor STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderMac.mm")
    target_compile_options(firefly-power-monitor PRIVATE -fobjc-arc)
    target_link_libraries(
        firefly-power-monitor PUBLIC "-framework AppKit" "-framework CoreGraphics"
                                     "-framework IOKit" "-framework CoreFoundation")
else()
    add_library(firefly-power-monitor STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderUnsupported.cpp")
endif()
if(APPLE)
    add_executable(firefly-power-provider-probe
                   "${FIREFLY_SOURCE_DIR}/firefly/tests/power-monitor/ProviderProbe.mm")
    target_compile_options(firefly-power-provider-probe PRIVATE -fobjc-arc)
else()
    add_executable(firefly-power-provider-probe
                   "${FIREFLY_SOURCE_DIR}/firefly/tests/power-monitor/UnsupportedProbe.cpp")
endif()
target_include_directories(firefly-power-provider-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-power-provider-probe PRIVATE firefly-power-monitor)
add_custom_target(
    check-firefly-power-provider
    COMMAND "$<TARGET_FILE:firefly-power-provider-probe>"
    DEPENDS firefly-power-provider-probe
    VERBATIM)
