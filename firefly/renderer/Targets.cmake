# This module is included by Inject.cmake in an upstream-root build. It is never added to the
# standalone Mite build or linked into the Mite executable.
add_library(firefly-preload-config STATIC
            "${FIREFLY_SOURCE_DIR}/firefly/native/preload/ProcessConfig.cpp")
target_link_libraries(firefly-preload-config PUBLIC AK)
target_include_directories(firefly-preload-config PUBLIC "${FIREFLY_SOURCE_DIR}/firefly")
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/vendor/ladybird.json")
file(READ "${FIREFLY_SOURCE_DIR}/vendor/ladybird.json" firefly_process_manifest)
string(JSON firefly_process_revision GET "${firefly_process_manifest}" revision)
add_library(
    firefly-renderer-process STATIC
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-helper/preload/Process.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-helper/preload/WebFrame.cpp")
target_include_directories(firefly-renderer-process
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
target_link_libraries(firefly-renderer-process PUBLIC firefly-preload-config LibWeb LibCore)
target_compile_definitions(firefly-renderer-process
                           PRIVATE FIREFLY_LIBJS_REVISION="${firefly_process_revision}")
include("${FIREFLY_SOURCE_DIR}/firefly/native/ipc/bootstrap/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/mite/cmake/ExecutionContextAdapter.cmake")
mite_adapt_execution_context("${FIREFLY_SOURCE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/LinuxFFmpeg.cmake")
firefly_hide_bundled_ffmpeg(LibMediaCodecs)
if(ENABLE_LTO_FOR_RELEASE AND NOT CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    message(FATAL_ERROR "Requested Release LTO is unsupported by this compiler/linker")
endif()
add_executable(firefly-renderer-probe "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
target_link_libraries(
    firefly-renderer-probe
    PRIVATE LibWebView
            LibIPC
            LibURL
            LibGfx
            LibCore
            LibMain
            AK)
target_include_directories(firefly-renderer-probe PRIVATE "${LADYBIRD_SOURCE_DIR}"
                                                          "${LADYBIRD_SOURCE_DIR}/Libraries")
target_compile_definitions(firefly-renderer-probe
                           PRIVATE FIREFLY_LADYBIRD_DIRECTORY="$<TARGET_FILE_DIR:ladybird>")
add_dependencies(firefly-renderer-probe ladybird ${ladybird_helper_processes})
add_custom_target(
    check-firefly-upstream
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/check_upstream.py"
    VERBATIM)
add_dependencies(firefly-renderer-probe check-firefly-upstream)
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/CallbackTargets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/loading/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/loading/web-request/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/canvas/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/canvas/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/autoplay/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/files/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/protocol/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/permissions/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/crash/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/ipc/bytes/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/native/protocol/files/Targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/loading/protocol/Targets.cmake")
# Retain upstream's process boundary: Skia must stay in Compositor, including when our frontend is
# added after the upstream root's own confinement check.
ladybird_check_skia_confinement(firefly-renderer-probe)
add_custom_target(
    check-firefly-renderer
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_renderer.py" --probe
            "$<TARGET_FILE:firefly-renderer-probe>"
    DEPENDS firefly-renderer-probe
    USES_TERMINAL VERBATIM)

if(APPLE
   OR WIN32
   OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${FIREFLY_SOURCE_DIR}/firefly/native/qt/Targets.cmake")
endif()

add_library(
    firefly-isolated-world STATIC
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/IsolatedWorld.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeData.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/BridgeWire.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/serialization/WireValue.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/context/FunctionBridge.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/context/Boundary.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/context/Snapshot.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/context/CallbackRegistry.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/context/CallbackEndpoint.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/context/ContextBridge.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/WorldJobGuard.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/jobs/Registration.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/jobs/Principal.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/jobs/EventValues.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/jobs/Handlers.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/jobs/Listeners.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/jobs/Traversal.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/jobs/Resources.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge/worlds/jobs/Dispatch.cpp")
target_include_directories(
    firefly-isolated-world PUBLIC "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge"
                                  "${LADYBIRD_SOURCE_DIR}" "${LADYBIRD_SOURCE_DIR}/Libraries")
target_include_directories(firefly-isolated-world PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-isolated-world PUBLIC LibWeb LibJS LibGC LibCore LibURL AK)
add_dependencies(firefly-isolated-world check-firefly-upstream)
include("${FIREFLY_SOURCE_DIR}/firefly/tests/message-ports/Targets.cmake")
add_executable(
    firefly-isolation-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/main.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/BridgeDataTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/FunctionBridgeTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/ContextBridgeTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/PublicationGetterTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/PreloadProcessTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/WebFrameTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/CallbackBridgeTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/RedactionBridgeTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/BinaryBridgeTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/PromiseBridgeTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/WorldJobGuardTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/HandlerTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/CustomEventTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/ErrorEventTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/MessageEventTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/PrincipalCallbackTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/NodeFilterTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/FileModulePolicyTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/FileOriginTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/FileRequestTest.cpp")
target_include_directories(firefly-isolation-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/renderer-helper")
target_link_libraries(
    firefly-isolation-probe PRIVATE LibIPC firefly-renderer-process firefly-isolated-world
                                    firefly-preload-config LibMain)
ladybird_check_skia_confinement(firefly-isolation-probe)
add_executable(
    firefly-wire-process-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/WireProcess.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/BridgeWireTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/BinaryWireTest.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/tests/isolation/BinaryViewTest.cpp")
target_link_libraries(firefly-wire-process-probe PRIVATE firefly-isolated-world
                                                         firefly-preload-config LibMain)
ladybird_check_skia_confinement(firefly-wire-process-probe)
add_custom_target(
    check-firefly-wire
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_wire_process.py"
            --probe "$<TARGET_FILE:firefly-wire-process-probe>"
    DEPENDS firefly-wire-process-probe check-firefly-byte-channel
    USES_TERMINAL VERBATIM)
add_custom_target(
    check-firefly-isolation
    COMMAND "$<TARGET_FILE:firefly-isolation-probe>"
    DEPENDS firefly-isolation-probe check-firefly-wire
    USES_TERMINAL VERBATIM)

# Exercise real media elements and their native activation paths without Qt, playable media or an OS
# audio output. Public preference admission is separate.
add_executable(
    firefly-autoplay-media-probe
    "${FIREFLY_SOURCE_DIR}/firefly/tests/autoplay/MediaProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/renderer-helper/autoplay/Session.cpp")
target_include_directories(
    firefly-autoplay-media-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge"
                                         "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(
    firefly-autoplay-media-probe
    PRIVATE LibWeb
            LibJS
            LibGC
            LibURL
            LibCore
            LibMain
            AK)
ladybird_check_skia_confinement(firefly-autoplay-media-probe)
add_custom_target(
    check-firefly-autoplay-media
    COMMAND "$<TARGET_FILE:firefly-autoplay-media-probe>"
    DEPENDS firefly-autoplay-media-probe
    VERBATIM)

# Actual Rust graph, Promise lifecycle and wall-clock null playback. No device factory is reachable
# from this diagnostic PageClient.
add_executable(firefly-autoplay-audio-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/autoplay/AudioProbe.cpp")
target_include_directories(firefly-autoplay-audio-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
target_link_libraries(
    firefly-autoplay-audio-probe
    PRIVATE LibWeb
            LibIPC
            LibWebCommon
            LibJS
            LibGC
            LibAudio
            LibMedia
            LibGfx
            LibURL
            LibCore
            LibMain
            AK)
ladybird_check_skia_confinement(firefly-autoplay-audio-probe)
add_custom_target(
    check-firefly-autoplay-audio
    COMMAND "$<TARGET_FILE:firefly-autoplay-audio-probe>"
    DEPENDS firefly-autoplay-audio-probe
    VERBATIM)

if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${FIREFLY_SOURCE_DIR}/firefly/renderer-helper/Targets.cmake")
endif()

# Native Windows console probes need the same owned runtime DLL deployment as upstream helpers. This
# does not enable the unfinished desktop/production runtime.
if(WIN32)
    target_link_libraries(firefly-renderer-probe PRIVATE shell32)
    foreach(probe firefly-renderer-probe firefly-isolation-probe firefly-wire-process-probe
                  firefly-autoplay-media-probe firefly-autoplay-audio-probe)
        ladybird_windows_bin(${probe} CONSOLE)
    endforeach()
endif()

include("${FIREFLY_SOURCE_DIR}/mite/cmake/SizeOptimization.cmake")
elytra_enable_size_optimization()

include("${FIREFLY_SOURCE_DIR}/firefly/native/SharedOpenSSL.cmake")
