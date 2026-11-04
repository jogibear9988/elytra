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
file(READ "${CMAKE_SOURCE_DIR}/vendor/ladybird.json" upstream_manifest)
string(JSON MITE_LADYBIRD_REVISION GET "${upstream_manifest}" revision)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/vendor/ladybird.json" "${CMAKE_SOURCE_DIR}/tooling/check_upstream.py")
configure_file(mite/runtime/UpstreamVersion.h.in generated/UpstreamVersion.h @ONLY)
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

file(READ "${CMAKE_SOURCE_DIR}/mite/runtime/bootstrap.js" MITE_BOOTSTRAP_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mite/runtime/bootstrap.js")
configure_file(mite/runtime/bootstrap.h.in generated/MiteBootstrap.h @ONLY)
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/events.js" MITE_EVENTS_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mite/modules/events.js")
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/process.js" MITE_PROCESS_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mite/modules/process.js")
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/path.js" MITE_PATH_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mite/modules/path.js")
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/buffer.js" MITE_BUFFER_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mite/modules/buffer.js")
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/fs.js" MITE_FS_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mite/modules/fs.js")
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/url.js" MITE_URL_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mite/modules/url.js")
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/net.js" MITE_NET_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mite/modules/net.js")
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/util.js" MITE_UTIL_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/mite/modules/util.js")
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/generated/stream.js" MITE_STREAM_SOURCE)
file(READ "${CMAKE_SOURCE_DIR}/mite/modules/generated/string-decoder.js" MITE_STRING_DECODER_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/mite/modules/generated/stream.js" "${CMAKE_SOURCE_DIR}/mite/modules/generated/string-decoder.js")
configure_file(mite/runtime/BuiltinSources.h.in generated/BuiltinSources.h @ONLY)
add_executable(mite mite/runtime/main.cpp mite/runtime/EventLoop.cpp mite/runtime/ModuleLoader.cpp mite/runtime/PackageResolver.cpp mite/runtime/Builtins.cpp mite/runtime/Process.cpp mite/runtime/FileSystem.cpp mite/runtime/URL.cpp mite/runtime/Encoding.cpp mite/runtime/Network.cpp)
add_custom_target(check-runtime-js
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/check_runtime_js.py" VERBATIM)
add_dependencies(mite check-ladybird check-runtime-js)
target_include_directories(mite PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(mite PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>)
target_link_libraries(mite PRIVATE LibJSStandalone AK LibCore LibGC LibURL LibTextCodec ICU::uc)
include(CTest)
if(BUILD_TESTING)
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
