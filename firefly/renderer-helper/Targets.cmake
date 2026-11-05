execute_process(COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_renderer_hooks.py"
    RESULT_VARIABLE firefly_hook_check)
if(NOT firefly_hook_check EQUAL 0)
    message(FATAL_ERROR "Review the Firefly renderer entry adapters before updating")
endif()
add_custom_target(check-firefly-renderer-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_renderer_hooks.py" VERBATIM)
# The helper prototype currently has a verified macOS staging/signing path only.
set(firefly_helper_bundle "${CMAKE_BINARY_DIR}/FireflyHelper.app/Contents")
add_executable(FireflyWebContent "${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SessionAdapter.cpp" "${CMAKE_CURRENT_LIST_DIR}/Session.cpp"
    "${LADYBIRD_SOURCE_DIR}/Services/RendererSandboxMacOS.cpp")
add_dependencies(FireflyWebContent check-firefly-upstream check-firefly-renderer-hooks)
target_link_libraries(FireflyWebContent PRIVATE webcontentservice firefly-isolated-world LibSandbox LibURL)
set_target_properties(FireflyWebContent PROPERTIES OUTPUT_NAME WebContent RUNTIME_OUTPUT_DIRECTORY "${firefly_helper_bundle}/MacOS")
ladybird_sign_helper_process(FireflyWebContent)
ladybird_check_skia_confinement(FireflyWebContent)
add_custom_target(stage-firefly-helper
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${firefly_helper_bundle}/MacOS"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/stage_renderer_resources.py"
        --source "$<TARGET_FILE_DIR:ladybird>/../Resources" --destination "${firefly_helper_bundle}/Resources"
    DEPENDS FireflyWebContent ladybird ${ladybird_helper_processes} VERBATIM)
foreach(helper IN LISTS ladybird_helper_processes)
    if(NOT helper STREQUAL "WebContent")
        add_custom_command(TARGET stage-firefly-helper POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E create_symlink "$<TARGET_FILE:${helper}>" "${firefly_helper_bundle}/MacOS/${helper}" VERBATIM)
    endif()
endforeach()
add_executable(firefly-preload-probe "${FIREFLY_SOURCE_DIR}/firefly/renderer/main.cpp")
target_link_libraries(firefly-preload-probe PRIVATE LibWebView LibIPC LibURL LibGfx LibCore LibMain AK)
target_include_directories(firefly-preload-probe PRIVATE "${LADYBIRD_SOURCE_DIR}" "${LADYBIRD_SOURCE_DIR}/Libraries")
target_compile_definitions(firefly-preload-probe PRIVATE FIREFLY_LADYBIRD_DIRECTORY="${firefly_helper_bundle}/MacOS")
add_dependencies(firefly-preload-probe stage-firefly-helper check-firefly-upstream)
ladybird_check_skia_confinement(firefly-preload-probe)
add_custom_target(check-firefly-renderer-helper
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/test_renderer_helper.py"
        --probe "$<TARGET_FILE:firefly-preload-probe>" --control "$<TARGET_FILE:firefly-renderer-probe>"
    DEPENDS firefly-preload-probe firefly-renderer-probe USES_TERMINAL VERBATIM)
