execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_renderer_hooks.py"
    RESULT_VARIABLE firefly_hook_check)
if(NOT firefly_hook_check EQUAL 0)
    message(FATAL_ERROR "Review the Firefly renderer entry adapters before updating")
endif()
add_custom_target(
    check-firefly-renderer-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_renderer_hooks.py"
    VERBATIM)
# Keep each platform's resource discovery and sandbox implementation native. Linux acceptance is
# experimental until the dedicated hosted gate passes.
include("${CMAKE_CURRENT_LIST_DIR}/Layout.cmake")
add_executable(
    FireflyWebContent
    "${CMAKE_CURRENT_LIST_DIR}/Entry.cpp" "${CMAKE_CURRENT_LIST_DIR}/UpstreamSessionAdapter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DiagnosticSession.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DiagnosticByteChannel.cpp" ${firefly_renderer_platform_sources})
add_dependencies(FireflyWebContent check-firefly-upstream check-firefly-renderer-hooks)
target_link_libraries(FireflyWebContent PRIVATE webcontentservice firefly-isolated-world LibSandbox
                                                LibURL firefly-byte-channel)
set_target_properties(
    FireflyWebContent PROPERTIES OUTPUT_NAME WebContent RUNTIME_OUTPUT_DIRECTORY
                                                        "${firefly_helper_binary_dir}")
ladybird_sign_helper_process(FireflyWebContent)
ladybird_check_skia_confinement(FireflyWebContent)
add_custom_target(
    stage-firefly-helper
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${firefly_helper_binary_dir}"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/stage_renderer_resources.py"
            --source "${firefly_resource_source}" --destination "${firefly_helper_resources}"
    DEPENDS FireflyWebContent ladybird ${ladybird_helper_processes}
    VERBATIM)
foreach(helper IN LISTS ladybird_helper_processes)
    if(NOT helper STREQUAL "WebContent")
        add_custom_command(
            TARGET stage-firefly-helper
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E create_symlink "$<TARGET_FILE:${helper}>"
                    "${firefly_helper_binary_dir}/${helper}"
            VERBATIM)
    endif()
endforeach()
add_executable(firefly-preload-probe "${FIREFLY_SOURCE_DIR}/firefly/renderer/main.cpp")
target_link_libraries(
    firefly-preload-probe
    PRIVATE LibWebView
            LibIPC
            LibURL
            LibGfx
            LibCore
            LibMain
            AK)
target_include_directories(firefly-preload-probe PRIVATE "${LADYBIRD_SOURCE_DIR}"
                                                         "${LADYBIRD_SOURCE_DIR}/Libraries")
target_compile_definitions(firefly-preload-probe
                           PRIVATE FIREFLY_LADYBIRD_DIRECTORY="${firefly_helper_binary_dir}")
add_dependencies(firefly-preload-probe stage-firefly-helper check-firefly-upstream)
ladybird_check_skia_confinement(firefly-preload-probe)
add_executable(firefly-byte-renderer-probe
               "${FIREFLY_SOURCE_DIR}/firefly/renderer/ByteRendererProbe.cpp")
target_link_libraries(firefly-byte-renderer-probe PRIVATE LibWebView firefly-byte-channel LibURL
                                                          LibGfx LibMain)
target_compile_definitions(
    firefly-byte-renderer-probe PRIVATE FIREFLY_LADYBIRD_DIRECTORY="${firefly_helper_binary_dir}"
                                        FIREFLY_UPSTREAM_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
add_dependencies(firefly-byte-renderer-probe stage-firefly-helper check-firefly-upstream)
ladybird_check_skia_confinement(firefly-byte-renderer-probe)
add_custom_target(
    check-firefly-byte-renderer
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_byte_renderer.py"
            --probe "$<TARGET_FILE:firefly-byte-renderer-probe>"
    DEPENDS firefly-byte-renderer-probe
    USES_TERMINAL VERBATIM)
add_custom_target(
    check-firefly-renderer-helper
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_renderer_helper.py"
        --probe "$<TARGET_FILE:firefly-preload-probe>" --control
        "$<TARGET_FILE:firefly-renderer-probe>"
    DEPENDS firefly-preload-probe firefly-renderer-probe check-firefly-byte-renderer
    USES_TERMINAL VERBATIM)

if(FIREFLY_BUILD_RUNTIME)
    add_executable(
        FireflyRuntimeWebContent
        "${CMAKE_CURRENT_LIST_DIR}/Entry.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/UpstreamSessionAdapter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/PreloadSession.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ipc/IpcEndpoint.cpp"
        ${firefly_renderer_platform_sources})
    add_dependencies(FireflyRuntimeWebContent check-firefly-upstream check-firefly-renderer-hooks)
    target_include_directories(FireflyRuntimeWebContent PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(FireflyRuntimeWebContent PRIVATE webcontentservice firefly-isolated-world
                                                           LibSandbox LibURL)
    set_target_properties(
        FireflyRuntimeWebContent PROPERTIES OUTPUT_NAME WebContent RUNTIME_OUTPUT_DIRECTORY
                                                                   "${firefly_runtime_binary_dir}")
    ladybird_sign_helper_process(FireflyRuntimeWebContent)
    ladybird_check_skia_confinement(FireflyRuntimeWebContent)
    add_custom_target(
        stage-firefly-runtime-helper
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${firefly_runtime_binary_dir}"
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/stage_renderer_resources.py"
                --source "${firefly_resource_source}" --destination "${firefly_runtime_resources}"
        DEPENDS FireflyRuntimeWebContent ladybird ${ladybird_helper_processes}
        VERBATIM)
    foreach(helper IN LISTS ladybird_helper_processes)
        if(NOT helper STREQUAL "WebContent")
            add_custom_command(
                TARGET stage-firefly-runtime-helper
                POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E create_symlink "$<TARGET_FILE:${helper}>"
                        "${firefly_runtime_binary_dir}/${helper}"
                VERBATIM)
        endif()
    endforeach()
    add_dependencies(firefly stage-firefly-runtime-helper check-firefly-js)
endif()
