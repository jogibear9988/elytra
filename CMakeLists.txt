cmake_minimum_required(VERSION 3.30)
project(mite VERSION 0.1.0 LANGUAGES C CXX)

# Reuse upstream library targets without configuring the browser dependency graph.
set(LADYBIRD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ladybird")
if(NOT EXISTS "${LADYBIRD_SOURCE_DIR}/Libraries/LibJS/CMakeLists.txt")
    message(FATAL_ERROR "Initialize Ladybird: git submodule update --init --recursive")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/check_upstream.py"
    RESULT_VARIABLE upstream_check_result OUTPUT_VARIABLE upstream_check_output ERROR_VARIABLE upstream_check_error)
if(NOT upstream_check_result EQUAL 0)
    message(FATAL_ERROR "${upstream_check_output}${upstream_check_error}")
endif()
add_custom_target(check-ladybird
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/check_upstream.py" VERBATIM)
list(APPEND CMAKE_MODULE_PATH "${LADYBIRD_SOURCE_DIR}/Meta/CMake")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared upstream libraries")
set(ENABLE_GUI_TARGETS OFF)
set(ENABLE_INSTALL_HEADERS OFF)
set(ENABLE_JS_JIT OFF CACHE BOOL "Enable upstream LibJS JIT")
set(ENABLE_LTO_FOR_RELEASE OFF)
set(ENABLE_LAGOM_CCACHE OFF)
set(LADYBIRD_VERSION "")
set(LADYBIRD_BROWSER_VERSION "${PROJECT_VERSION}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(APPLE)
    enable_language(OBJCXX)
endif()
include(compile_options)
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
foreach(library LibCore LibCrypto LibGC LibJS LibRegex LibSyntax LibTextCodec LibThreading LibUnicode LibURL)
    add_subdirectory("${LADYBIRD_SOURCE_DIR}/Libraries/${library}" "Libraries/${library}")
endforeach()

include(mite/cmake/RustAggregate.cmake)
mite_aggregate_rust()

include(mite/cmake/Runtime.cmake)
mite_add_runtime(mite-runtime "${CMAKE_SOURCE_DIR}" LibJSStandalone)
add_executable(mite mite/runtime/main.cpp)
target_link_libraries(mite PRIVATE mite-runtime)
include(CTest)
if(BUILD_TESTING)
    add_test(NAME mite-rust-lock COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/check_mite_rust_lock.py")
    add_test(NAME mite-rust-lock-guard COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_mite_rust_lock.py")
    add_executable(mite-embedded-host mite/tests/embedding/Host.cpp)
    target_link_libraries(mite-embedded-host PRIVATE mite-runtime)
    # The documented build target must produce every enabled CTest fixture.
    add_dependencies(mite mite-embedded-host)
    add_test(NAME mite-host-modules COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_host_modules.py" --host "$<TARGET_FILE:mite-embedded-host>" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-embedding COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_embedding.py" --host "$<TARGET_FILE:mite-embedded-host>" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-runtime-js COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/check_runtime_js.py")
    add_test(NAME mite-upstream COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/check_upstream.py")
    add_test(NAME mite-upstream-guard COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/test_upstream.py")
    add_test(NAME mite-behavior COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_runtime.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-modules COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_modules.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-commonjs COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_commonjs.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-packages COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_packages.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-builtins COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_builtins.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-ticks COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_ticks.py" --mite "$<TARGET_FILE:mite>")
    add_executable(mite-os-netmask-probe mite/tests/os/netmask.cpp)
    target_include_directories(mite-os-netmask-probe PRIVATE "${CMAKE_SOURCE_DIR}/mite/runtime")
    add_dependencies(mite mite-os-netmask-probe)
    add_test(NAME mite-os-netmask COMMAND "$<TARGET_FILE:mite-os-netmask-probe>")
    add_test(NAME mite-crypto-hash COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_crypto_hash.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-crypto-random COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_crypto_random.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-os-constants COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_os_constants.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-os-priority COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_os_priority.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-os-network COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_os_network.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-os-data COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_os_data.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-os COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_os.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-http COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_http.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-http-external COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_http_external.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-http-headers COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_http_headers.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-dns COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_dns.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-dns-validation COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_dns_validation.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-net COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_net.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-net-addresses COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_net_addresses.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-streams COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_streams.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-string-decoder COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_string_decoder.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-encoding COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_encoding.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-url COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_url.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-url-paths COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_url_paths.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-filesystem COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_fs.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-buffer COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_buffer.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-path COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_path.py" --mite "$<TARGET_FILE:mite>")
    add_test(NAME mite-npm COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tooling/test_npm.py" --mite "$<TARGET_FILE:mite>")
endif()
install(TARGETS mite RUNTIME DESTINATION bin)
