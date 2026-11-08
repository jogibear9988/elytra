cmake_minimum_required(VERSION 3.30)
project(
    mite
    VERSION 0.1.0
    LANGUAGES C CXX)

# Reuse upstream library targets without configuring the browser dependency graph.
set(LADYBIRD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ladybird")
if(NOT EXISTS "${LADYBIRD_SOURCE_DIR}/Libraries/LibJS/CMakeLists.txt")
    message(FATAL_ERROR "Initialize Ladybird: git submodule update --init --recursive")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/check_upstream.py"
    RESULT_VARIABLE upstream_check_result
    OUTPUT_VARIABLE upstream_check_output
    ERROR_VARIABLE upstream_check_error)
if(NOT upstream_check_result EQUAL 0)
    message(FATAL_ERROR "${upstream_check_output}${upstream_check_error}")
endif()
add_custom_target(
    check-ladybird
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/check_upstream.py"
    VERBATIM)
list(APPEND CMAKE_MODULE_PATH "${LADYBIRD_SOURCE_DIR}/Meta/CMake")
set(BUILD_SHARED_LIBS
    OFF
    CACHE BOOL "Build shared upstream libraries")
set(ENABLE_GUI_TARGETS OFF)
set(ENABLE_INSTALL_HEADERS OFF)
set(ENABLE_JS_JIT
    OFF
    CACHE BOOL "Enable upstream LibJS JIT")
set(ENABLE_LTO_FOR_RELEASE
    OFF
    CACHE BOOL "Enable C++ link-time optimization in Release builds")
option(ENABLE_CI_BASELINE_CPU "Use upstream's common CPU baseline for distribution builds" OFF)
set(ENABLE_LAGOM_CCACHE OFF)
set(LADYBIRD_VERSION "")
set(LADYBIRD_BROWSER_VERSION "${PROJECT_VERSION}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(APPLE)
    enable_language(OBJCXX)
endif()
include(compile_options)
if(ENABLE_LTO_FOR_RELEASE
   AND CMAKE_BUILD_TYPE STREQUAL "Release"
   AND NOT CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    message(FATAL_ERROR "Requested Release LTO is unsupported by this compiler/linker")
endif()
include(lagom_install_options)
include(rust_crate)
include(targets)
add_library(JSClangPlugin INTERFACE)
add_library(GenericClangPlugin INTERFACE)
add_lagom_library_install_rules(JSClangPlugin)
add_lagom_library_install_rules(GenericClangPlugin)

find_package(libuv CONFIG REQUIRED)
find_package(llhttp CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(FastFloat CONFIG REQUIRED)
find_package(simdutf CONFIG REQUIRED)
find_package(zmij CONFIG REQUIRED)
find_package(mimalloc CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ICU 78.3 EXACT REQUIRED COMPONENTS data i18n uc)
find_path(LLVM_LIBC_INCLUDE_DIR NAMES llvm-libc/shared/math.h REQUIRED)
pkg_check_modules(LIBPSL REQUIRED IMPORTED_TARGET libpsl)
pkg_check_modules(libtommath REQUIRED IMPORTED_TARGET libtommath)
if(WIN32)
    find_package(pthread REQUIRED)
    find_package(mman REQUIRED)
endif()
include_directories("${LADYBIRD_SOURCE_DIR}" "${LADYBIRD_SOURCE_DIR}/Libraries"
                    "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/Libraries")
add_subdirectory("${LADYBIRD_SOURCE_DIR}/AK" AK)
foreach(
    library
    LibCore
    LibCrypto
    LibGC
    LibJS
    LibRegex
    LibSyntax
    LibTextCodec
    LibThreading
    LibUnicode
    LibURL)
    add_subdirectory("${LADYBIRD_SOURCE_DIR}/Libraries/${library}" "Libraries/${library}")
endforeach()

include(mite/cmake/RustAggregate.cmake)
mite_aggregate_rust()
include(mite/cmake/ExecutionContextAdapter.cmake)
mite_adapt_execution_context("${CMAKE_SOURCE_DIR}")

include(mite/cmake/Runtime.cmake)
mite_add_runtime(mite-runtime "${CMAKE_SOURCE_DIR}" LibJSStandalone)
add_executable(mite mite/cli/main.cpp)
target_link_libraries(mite PRIVATE mite-runtime)
include(CTest)
if(BUILD_TESTING)
    add_executable(firefly-startup-probe firefly/native/platform/StartupProbe.cpp)
    add_dependencies(mite firefly-startup-probe)
    add_test(NAME firefly-startup-options COMMAND "$<TARGET_FILE:firefly-startup-probe>")
    if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME firefly-launcher-options
                 COMMAND "${Python3_EXECUTABLE}"
                         "${CMAKE_SOURCE_DIR}/tooling/tests/test_launcher_options.py")
    endif()
    add_executable(mite-execution-context-probe mite/tests/embedding/ExecutionContext.cpp)
    target_link_libraries(mite-execution-context-probe PRIVATE LibJSStandalone)
    add_dependencies(mite mite-execution-context-probe)
    add_test(NAME mite-execution-context COMMAND "$<TARGET_FILE:mite-execution-context-probe>")
    add_test(NAME mite-rust-lock COMMAND "${Python3_EXECUTABLE}"
                                         "${CMAKE_SOURCE_DIR}/tooling/check_mite_rust_lock.py")
    add_test(NAME mite-rust-lock-guard
             COMMAND "${Python3_EXECUTABLE}"
                     "${CMAKE_SOURCE_DIR}/tooling/tests/test_mite_rust_lock.py")
    add_executable(mite-embedded-host mite/tests/embedding/Host.cpp)
    target_link_libraries(mite-embedded-host PRIVATE mite-runtime)
    # The documented build target must produce every enabled CTest fixture.
    add_dependencies(mite mite-embedded-host)
    if(WIN32)
        add_executable(mite-startup-probe mite/tests/embedding/WindowsStartup.cpp)
        target_link_libraries(mite-startup-probe PRIVATE mite-runtime Dbghelp)
        add_dependencies(mite mite-startup-probe)
    endif()
    add_test(
        NAME mite-host-modules
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_host_modules.py"
                --host "$<TARGET_FILE:mite-embedded-host>" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-embedding
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_embedding.py"
                     --host "$<TARGET_FILE:mite-embedded-host>" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-runtime-js COMMAND "${Python3_EXECUTABLE}"
                                          "${CMAKE_SOURCE_DIR}/tooling/check_runtime_js.py")
    add_test(NAME firefly-renderer-platforms
             COMMAND "${Python3_EXECUTABLE}"
                     "${CMAKE_SOURCE_DIR}/tooling/tests/test_renderer_platforms.py")
    add_test(NAME firefly-release-artifacts
             COMMAND "${Python3_EXECUTABLE}"
                     "${CMAKE_SOURCE_DIR}/tooling/tests/test_release_artifacts.py")
    add_test(NAME mite-upstream COMMAND "${Python3_EXECUTABLE}"
                                        "${CMAKE_SOURCE_DIR}/tooling/check_upstream.py")
    add_test(NAME mite-upstream-guard COMMAND "${Python3_EXECUTABLE}"
                                              "${CMAKE_SOURCE_DIR}/tooling/tests/test_upstream.py")
    add_test(NAME mite-behavior
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_runtime.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-modules
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_modules.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-module-utilities
        COMMAND
            "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_module_utilities.py"
            --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(
        NAME mite-module-paths
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_module_paths.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-util-inherits
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_inherits.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-util-promisify
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_promisify.py"
                     --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(
        NAME mite-module-hooks
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_module_hooks.py"
                --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(
        NAME mite-host-filesystem
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_original_fs.py"
                --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(NAME mite-commonjs
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_commonjs.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-child-process
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_child_process.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-exec-file
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_exec_file.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-exec
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_exec.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-exec-gc
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_exec_gc.py"
                     --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(NAME mite-packages
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_packages.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-builtins
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_builtins.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-lifetime
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_lifetime.py"
                     --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(NAME mite-abort
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_abort.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-process-timing
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_process_timing.py"
                --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(
        NAME mite-process-stdio
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_process_stdio.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-perf-hooks
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_perf_hooks.py"
                     --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(
        NAME mite-process-events
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_process_events.py"
                --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(NAME mite-assert
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_assert.py"
                     --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(NAME mite-readline
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_readline.py"
                     --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(
        NAME mite-code-oss-os-release
        COMMAND
            "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_code_oss_os_release.py"
            --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-code-oss-bootstrap
        COMMAND
            "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_code_oss_bootstrap.py"
            --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-code-oss-platform
        COMMAND
            "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_code_oss_platform.py"
            --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-code-oss-cache
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_code_oss_cache.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-code-oss-engine
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_code_oss_engine.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-code-oss-local-desktop
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tooling/tests/test_code_oss_local_desktop.py" --mite
            "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-code-oss-nls
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_code_oss_nls.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-code-oss-dependencies
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tooling/tests/test_code_oss_dependencies.py" --mite
            "$<TARGET_FILE:mite>")
    add_test(
        NAME firefly-view-policy
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_view_policy.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(NAME firefly-screen-policy
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_screen.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME firefly-clipboard-policy
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_clipboard.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME firefly-theme-policy
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_theme.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME firefly-window-geometry-policy
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_window_geometry.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-web-streams
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_web_streams.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME firefly-rust-origin-driver
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/rust-origin/test_driver.py"
                --compiler "${CMAKE_CXX_COMPILER}")
    add_test(NAME firefly-protocol-origin-adapter
             COMMAND "${Python3_EXECUTABLE}"
                     "${CMAKE_SOURCE_DIR}/tooling/prepare_protocol_origin_hooks.py")
    add_test(
        NAME mite-web-adapters
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_web_adapters.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-fetch
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_fetch.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-fetch-data
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_fetch_data.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-zlib
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_zlib.py"
                     --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(
        NAME mite-compression
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_compression.py"
                --mite "$<TARGET_FILE:mite>" --host "$<TARGET_FILE:mite-embedded-host>")
    add_test(NAME mite-runtime-line-endings
             COMMAND "${Python3_EXECUTABLE}"
                     "${CMAKE_SOURCE_DIR}/tooling/tests/test_runtime_line_endings.py")
    add_test(NAME mite-runtime-notices
             COMMAND "${Python3_EXECUTABLE}"
                     "${CMAKE_SOURCE_DIR}/tooling/tests/test_runtime_notices.py")
    add_test(NAME mite-immediates
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_immediates.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-ticks
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_ticks.py"
                     --mite "$<TARGET_FILE:mite>")
    add_executable(mite-os-netmask-probe mite/tests/os/netmask.cpp)
    target_include_directories(mite-os-netmask-probe PRIVATE "${CMAKE_SOURCE_DIR}/mite/runtime")
    add_dependencies(mite mite-os-netmask-probe)
    add_test(NAME mite-os-netmask COMMAND "$<TARGET_FILE:mite-os-netmask-probe>")
    add_test(
        NAME mite-crypto-hmac
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_crypto_hmac.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-crypto-hash
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_crypto_hash.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-crypto-random
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_crypto_random.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-crypto-random-int
        COMMAND
            "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_crypto_random_int.py"
            --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-http-client
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_http_client.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-os-constants
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_os_constants.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-os-priority
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_os_priority.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-os-network
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_os_network.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-os-data
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_os_data.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-os
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_os.py" --mite
                     "$<TARGET_FILE:mite>")
    add_test(NAME mite-http
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_http.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-http-external
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_http_external.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-http-headers
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_http_headers.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-dns
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_dns.py" --mite
                     "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-dns-validation
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_dns_validation.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-net-abort
        COMMAND
            "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tooling/tests/test_net_abort.py"
            --mite $<TARGET_FILE:mite>)
    add_test(NAME mite-http-abort
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_http_abort.py"
                     --mite $<TARGET_FILE:mite>)
    add_test(NAME mite-tls
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_tls.py" --mite
                     $<TARGET_FILE:mite> --host $<TARGET_FILE:mite-embedded-host>)
    add_test(NAME mite-https
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_https.py"
                     --mite $<TARGET_FILE:mite> --host $<TARGET_FILE:mite-embedded-host>)
    add_test(NAME mite-net
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_net.py" --mite
                     "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-net-addresses
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_net_addresses.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-streams
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_streams.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-string-decoder
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_string_decoder.py"
                --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-encoding
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_encoding.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-url
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_url.py" --mite
                     "$<TARGET_FILE:mite>")
    add_test(NAME mite-url-paths
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_url_paths.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-filesystem
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_fs.py" --mite
                     "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-file-write-stream
        COMMAND
            "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_file_write_stream.py"
            --mite "$<TARGET_FILE:mite>")
    add_test(
        NAME mite-file-write-stream-gc
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tooling/tests/test_file_write_stream_gc.py" --host
            "$<TARGET_FILE:mite-embedded-host>")
    add_test(NAME mite-buffer
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_buffer.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-path
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_path.py"
                     --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-npm
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/tests/test_npm.py" --mite
                     "$<TARGET_FILE:mite>")
endif()
include(mite/cmake/SizeOptimization.cmake)
elytra_enable_size_optimization()
install(TARGETS mite RUNTIME DESTINATION bin)
