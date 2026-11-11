if(APPLE)
    add_library(firefly-recent-documents STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderMac.mm")
    target_compile_options(firefly-recent-documents PRIVATE -fobjc-arc)
    target_link_libraries(firefly-recent-documents PUBLIC "-framework AppKit"
                                                          "-framework Foundation")
else()
    add_library(firefly-recent-documents STATIC "${CMAKE_CURRENT_LIST_DIR}/ProviderUnsupported.cpp")
endif()
