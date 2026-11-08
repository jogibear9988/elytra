if(APPLE)
    add_library(firefly-system-preferences STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderMac.mm")
    target_compile_options(firefly-system-preferences PRIVATE -fobjc-arc)
    target_link_libraries(
        firefly-system-preferences PUBLIC "-framework AppKit" "-framework Foundation"
                                          "-framework AVFoundation" "-framework CoreGraphics")
else()
    add_library(firefly-system-preferences STATIC
                "${CMAKE_CURRENT_LIST_DIR}/ProviderUnsupported.cpp")
endif()
