include_guard(GLOBAL)
if(NOT WIN32)
    message(FATAL_ERROR "Native AppContainer helper integration is Windows-only")
endif()
include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
if(NOT DEFINED FIREFLY_WINDOWS_HELPER_OVERLAY)
    message(FATAL_ERROR "Native Windows helper requires the early helper overlay")
endif()
# The early Inject hook selects one composed broker header before any SDK directory is declared.
# Never enable the launch component with an unhardened broker or a mixed original/composed class
# declaration.
if(NOT DEFINED FIREFLY_WINDOWS_BROKER_OVERLAY
   OR NOT EXISTS "${FIREFLY_WINDOWS_BROKER_OVERLAY}/LibIPC/HandleBroker.h")
    message(FATAL_ERROR "Native Windows helper requires the early hardened broker overlay")
endif()
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/windows-sandbox/broker.py"
            --output "${FIREFLY_WINDOWS_BROKER_OVERLAY}" RESULT_VARIABLE windows_broker_composition)
if(NOT windows_broker_composition EQUAL 0)
    message(FATAL_ERROR "Review native Windows helper broker source ownership")
endif()
firefly_replace_source(LibIPC "Libraries/LibIPC/HandleBroker.cpp"
                       "${FIREFLY_WINDOWS_BROKER_OVERLAY}/HandleBroker.cpp")
firefly_replace_source(LibIPC "Libraries/LibIPC/HandleBrokerServer.cpp"
                       "${FIREFLY_WINDOWS_BROKER_OVERLAY}/HandleBrokerServer.cpp")
add_library(
    firefly-windows-helper-launch SHARED
    "${CMAKE_CURRENT_LIST_DIR}/SDK.cpp" "${CMAKE_CURRENT_LIST_DIR}/Resources.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Inventory.cpp" "${CMAKE_CURRENT_LIST_DIR}/../Sandbox.cpp")
file(READ "${FIREFLY_SOURCE_DIR}/vendor/ladybird.json" windows_helper_manifest)
string(JSON windows_helper_revision GET "${windows_helper_manifest}" revision)
target_compile_definitions(
    firefly-windows-helper-launch
    PRIVATE FIREFLY_WINDOWS_HELPER_LAUNCH_BUILD
            FIREFLY_WINDOWS_HELPER_REVISION="${windows_helper_revision}"
            UNICODE
            _UNICODE
            _WIN32_WINNT=0x0A00
            NOMINMAX
            WIN32_LEAN_AND_MEAN)
target_include_directories(
    firefly-windows-helper-launch
    PRIVATE "${LADYBIRD_SOURCE_DIR}" "${LADYBIRD_SOURCE_DIR}/Libraries" "${CMAKE_BINARY_DIR}"
            "${CMAKE_BINARY_DIR}/Libraries")
target_link_libraries(
    firefly-windows-helper-launch
    PRIVATE AK
            userenv
            bcrypt
            advapi32
            shell32
            ole32
            uuid)
# One shared native owner/TLS registry across Core and WebView DLLs. Linking two static copies would
# lose scoped authority and exact HANDLE lease ownership.
target_link_libraries(LibCore PRIVATE firefly-windows-helper-launch)
target_link_libraries(LibWebView PRIVATE firefly-windows-helper-launch)
firefly_replace_source(LibCore "Libraries/LibCore/ProcessWindows.cpp"
                       "${FIREFLY_WINDOWS_HELPER_OVERLAY}/ProcessWindows.cpp")
firefly_replace_source(LibWebView "Libraries/LibWebView/Process.cpp"
                       "${FIREFLY_WINDOWS_HELPER_OVERLAY}/Process.cpp")
firefly_replace_source(ImageDecoder "Services/ImageDecoder/SandboxUnimplemented.cpp"
                       "${CMAKE_CURRENT_LIST_DIR}/ImageDecoderSandbox.cpp")
target_link_libraries(ImageDecoder PRIVATE firefly-windows-helper-launch)
firefly_replace_source(Compositor "Services/Compositor/SandboxUnimplemented.cpp"
                       "${CMAKE_CURRENT_LIST_DIR}/CompositorSandbox.cpp")
target_link_libraries(Compositor PRIVATE firefly-windows-helper-launch)
firefly_replace_source(LibWebView "Libraries/LibWebView/SystemProbeClient.cpp"
                       "${FIREFLY_WINDOWS_HELPER_OVERLAY}/SystemProbeClient.cpp")
firefly_replace_source(SystemProbe "Services/SystemProbe/main.cpp"
                       "${FIREFLY_WINDOWS_HELPER_OVERLAY}/SystemProbeMain.cpp")
target_link_libraries(SystemProbe PRIVATE firefly-windows-helper-launch)
# The compiled helper slice is not the complete desktop runtime. The default runtime/release gate
# must remain closed until every real helper role is covered.
set_property(TARGET firefly-windows-helper-launch PROPERTY FIREFLY_WINDOWS_HELPER_ROLES_COMPLETE
                                                           FALSE)
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/windows-sandbox/prepare.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibCore/ProcessWindows.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/Process.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/SystemProbeClient.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/SystemProbeClient.h"
             "${LADYBIRD_SOURCE_DIR}/Services/SystemProbe/main.cpp"
             "${FIREFLY_SOURCE_DIR}/tooling/windows-sandbox/broker.py"
             "${FIREFLY_SOURCE_DIR}/tooling/windows-sandbox/stage.py"
             "${FIREFLY_SOURCE_DIR}/firefly/native/sandbox/windows/helper/BrokerPolicy.h"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibIPC/HandleBroker.h"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibIPC/HandleBroker.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibIPC/HandleBrokerServer.cpp")

# The producer remains in the SDK bin directory. Only a sealed physical copy is executable through
# the helper launch boundary; no build-tree symlink is admitted.
function(firefly_stage_windows_helper target producer binary_directory)
    get_filename_component(runtime_root "${binary_directory}" DIRECTORY)
    set(inputs
        "WebContent.exe|$<TARGET_FILE:${producer}>\n$<JOIN:$<TARGET_RUNTIME_DLLS:${producer}>,\n>\n"
    )
    foreach(helper IN LISTS ladybird_helper_processes)
        if(NOT helper STREQUAL "WebContent")
            string(APPEND inputs
                   "$<TARGET_FILE:${helper}>\n$<JOIN:$<TARGET_RUNTIME_DLLS:${helper}>,\n>\n")
        endif()
    endforeach()
    set(input_file "${CMAKE_BINARY_DIR}/firefly-adapters/${target}-$<CONFIG>.txt")
    file(
        GENERATE
        OUTPUT "${input_file}"
        CONTENT "${inputs}")
    add_custom_target(
        ${target}
        COMMAND
            "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/windows-sandbox/stage.py" --root
            "${runtime_root}" --targets "${input_file}" --resources "${firefly_resource_source}"
            --revision "${windows_helper_revision}"
        DEPENDS ${producer} ladybird ${ladybird_helper_processes}
        VERBATIM)
endfunction()

# Actual native Core/AppContainer/broker traffic. This component is deliberately separate from
# LibWeb rendering and from the still-closed desktop runtime.
add_executable(firefly-windows-broker-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/sandbox/windows/BrokerProbe.cpp")
target_include_directories(firefly-windows-broker-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(
    firefly-windows-broker-probe
    PRIVATE firefly-windows-helper-launch
            LibIPC
            LibCore
            LibThreading
            LibMain
            LibWebView
            LibImageDecoderClient
            LibGfx
            LibPlatformFeatures
            AK)
ladybird_windows_bin(firefly-windows-broker-probe CONSOLE)
set(windows_broker_inputs "${CMAKE_BINARY_DIR}/firefly-adapters/windows-broker-probe-$<CONFIG>.txt")
file(
    GENERATE
    OUTPUT "${windows_broker_inputs}"
    CONTENT
        "WebContent.exe|$<TARGET_FILE:firefly-windows-broker-probe>\n$<JOIN:$<TARGET_RUNTIME_DLLS:firefly-windows-broker-probe>,\n>\n$<TARGET_FILE:ImageDecoder>\n$<JOIN:$<TARGET_RUNTIME_DLLS:ImageDecoder>,\n>\n$<TARGET_FILE:Compositor>\n$<JOIN:$<TARGET_RUNTIME_DLLS:Compositor>,\n>\n$<TARGET_FILE:SystemProbe>\n$<JOIN:$<TARGET_RUNTIME_DLLS:SystemProbe>,\n>\n"
)
add_custom_target(
    check-firefly-windows-broker
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_windows_broker.py" --probe
        "$<TARGET_FILE:firefly-windows-broker-probe>" --targets "${windows_broker_inputs}"
        --resources "${CMAKE_BINARY_DIR}/share/Lagom" --revision "${windows_helper_revision}"
    DEPENDS firefly-windows-broker-probe ImageDecoder Compositor SystemProbe ladybird
    USES_TERMINAL VERBATIM)
