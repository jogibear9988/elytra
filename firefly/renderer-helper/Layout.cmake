# Native helper/resource discovery. Keep unsupported sandbox platforms explicit.
if(APPLE)
    set(firefly_helper_binary_dir "${CMAKE_BINARY_DIR}/FireflyHelper.app/Contents/MacOS")
    set(firefly_helper_resources "${CMAKE_BINARY_DIR}/FireflyHelper.app/Contents/Resources")
    set(firefly_runtime_binary_dir "${CMAKE_BINARY_DIR}/FireflyRuntimeHelper.app/Contents/MacOS")
    set(firefly_runtime_resources "${CMAKE_BINARY_DIR}/FireflyRuntimeHelper.app/Contents/Resources")
    set(firefly_resource_source "$<TARGET_FILE_DIR:ladybird>/../Resources")
    set(firefly_renderer_platform_sources
        "${LADYBIRD_SOURCE_DIR}/Services/RendererSandboxMacOS.cpp")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(firefly_helper_binary_dir "${CMAKE_BINARY_DIR}/firefly-helper/bin")
    set(firefly_helper_resources "${CMAKE_BINARY_DIR}/firefly-helper/share/Lagom")
    set(firefly_runtime_binary_dir "${CMAKE_BINARY_DIR}/firefly-runtime/bin")
    set(firefly_runtime_resources "${CMAKE_BINARY_DIR}/firefly-runtime/share/Lagom")
    set(firefly_resource_source "${CMAKE_BINARY_DIR}/share/Lagom")
    set(firefly_renderer_platform_sources "${LADYBIRD_SOURCE_DIR}/Services/RendererSandboxLinux.cpp"
                                          "${LADYBIRD_SOURCE_DIR}/Services/LeakSanitizer.cpp")
elseif(WIN32 AND FIREFLY_EXPERIMENTAL_WINDOWS_HELPER_LAUNCH)
    if(NOT TARGET firefly-windows-helper-launch)
        message(FATAL_ERROR "Native Windows helper launch authority is missing")
    endif()
    # This selects only the real, partial WebContent sandbox component. The full-runtime gate
    # separately requires every helper role to be covered.
    set(firefly_helper_binary_dir "${CMAKE_BINARY_DIR}/firefly-helper/bin")
    set(firefly_helper_resources "${CMAKE_BINARY_DIR}/firefly-helper/share/Lagom")
    set(firefly_runtime_binary_dir "${CMAKE_BINARY_DIR}/firefly-runtime/bin")
    set(firefly_runtime_resources "${CMAKE_BINARY_DIR}/firefly-runtime/share/Lagom")
    set(firefly_resource_source "${CMAKE_BINARY_DIR}/share/Lagom")
    set(firefly_renderer_platform_sources
        "${FIREFLY_SOURCE_DIR}/firefly/native/sandbox/windows/helper/RendererSandbox.cpp")
else()
    message(FATAL_ERROR "No verified native helper layout/sandbox adapter for this platform")
endif()
