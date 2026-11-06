# Reuse the pinned Linux view, X11 chrome and accessibility implementation. This presents a real
# window; it does not implement the Firefly desktop OS services.
include("${CMAKE_CURRENT_LIST_DIR}/Requirements.cmake")
firefly_require_wayland_native_interfaces()
find_package(PkgConfig REQUIRED)
pkg_check_modules(FIREFLY_XCB_ICCCM REQUIRED IMPORTED_TARGET xcb-icccm)
target_sources(
    firefly-qt-view
    PRIVATE "${LADYBIRD_SOURCE_DIR}/UI/Qt/AccessibilityInterface.cpp"
            "${LADYBIRD_SOURCE_DIR}/UI/Qt/NativeWindowContainer.cpp"
            "${LADYBIRD_SOURCE_DIR}/UI/Qt/X11Window.cpp" "${LADYBIRD_SOURCE_DIR}/UI/Qt/X11Window.h")
target_link_libraries(firefly-qt-view PRIVATE PkgConfig::FIREFLY_XCB_ICCCM)

if(USE_VULKAN_DMABUF_IMAGES)
    # Upstream already owns the shader generation commands. Depend on those exact outputs rather
    # than copying shaders or creating a second generation race.
    if(NOT TARGET generate_web_content_view_linux_shaders)
        message(FATAL_ERROR "Review upstream Linux view shader-generation target")
    endif()
    get_target_property(firefly_upstream_qt_binary_dir ladybird_impl BINARY_DIR)
    target_sources(firefly-qt-view PRIVATE "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentViewLinux.cpp")
    target_include_directories(firefly-qt-view PRIVATE "${firefly_upstream_qt_binary_dir}")
    target_link_libraries(firefly-qt-view PRIVATE Vulkan::Vulkan)
    add_dependencies(firefly-qt-view generate_web_content_view_linux_shaders)
endif()
