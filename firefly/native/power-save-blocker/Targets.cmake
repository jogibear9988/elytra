# Native assertion ownership is independent of VM bindings and desktop widgets.
if(APPLE)
    add_library(firefly-power-save-blocker STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderMac.mm")
    target_compile_options(firefly-power-save-blocker PRIVATE -fobjc-arc)
    target_link_libraries(firefly-power-save-blocker PUBLIC "-framework IOKit"
                                                            "-framework CoreFoundation")
else()
    add_library(firefly-power-save-blocker STATIC
                "${CMAKE_CURRENT_LIST_DIR}/ProviderUnsupported.cpp")
endif()
