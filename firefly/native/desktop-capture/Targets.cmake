# Provider owns OS metadata only; VM callback marshalling belongs to the main bridge.
if(APPLE)
    add_library(firefly-desktop-capture STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderMac.mm")
    target_compile_options(firefly-desktop-capture PRIVATE -fobjc-arc)
    target_link_libraries(
        firefly-desktop-capture PUBLIC "-framework Foundation" "-framework CoreGraphics"
                                       "-framework ScreenCaptureKit")
else()
    add_library(firefly-desktop-capture STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderUnsupported.cpp")
endif()
add_executable(firefly-desktop-capture-provider-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/desktop-capture/ProviderProbe.cpp")
target_include_directories(firefly-desktop-capture-provider-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-desktop-capture-provider-probe PRIVATE firefly-desktop-capture)
add_custom_target(
    check-firefly-desktop-capture-provider
    COMMAND "$<TARGET_FILE:firefly-desktop-capture-provider-probe>"
    DEPENDS firefly-desktop-capture-provider-probe
    VERBATIM)
