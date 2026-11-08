# Real OS registrations; unsupported platforms never fall back to application-local shortcuts.
if(APPLE)
    add_library(firefly-global-shortcut STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderMac.mm")
    target_compile_options(firefly-global-shortcut PRIVATE -fobjc-arc)
    target_link_libraries(firefly-global-shortcut PUBLIC "-framework Carbon"
                                                         "-framework Foundation")
else()
    add_library(firefly-global-shortcut STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderUnsupported.cpp")
endif()
