# RandR is a native Linux dependency. Do not substitute synthetic IDs on other hosts.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FIREFLY_XCB_RANDR REQUIRED IMPORTED_TARGET xcb-randr)
    add_library(firefly-x11-screen STATIC "${CMAKE_CURRENT_LIST_DIR}/ScreenX11.cpp")
    target_link_libraries(firefly-x11-screen PUBLIC Qt::Gui PkgConfig::FIREFLY_XCB_RANDR)
    add_executable(firefly-x11-screen-probe "${CMAKE_CURRENT_LIST_DIR}/X11Probe.cpp")
    target_link_libraries(firefly-x11-screen-probe PRIVATE firefly-x11-screen)
endif()
