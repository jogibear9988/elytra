# Passive native status and conservative proxy discovery; no endpoint traffic.
if(APPLE)
    add_library(firefly-network-state STATIC
                "${CMAKE_CURRENT_LIST_DIR}/../platform/NetworkStateMac.mm")
    target_compile_options(firefly-network-state PRIVATE -fobjc-arc)
    target_link_libraries(
        firefly-network-state PUBLIC "-framework Network" "-framework SystemConfiguration"
                                     "-framework Foundation")
else()
    add_library(firefly-network-state STATIC
                "${CMAKE_CURRENT_LIST_DIR}/../platform/NetworkStateUnsupported.cpp")
endif()
