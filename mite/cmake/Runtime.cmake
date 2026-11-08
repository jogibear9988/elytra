# One runtime source/generation graph for the standalone CLI and native hosts. The caller chooses
# the upstream JS target: a host already using LibWeb must not accidentally link a second standalone
# LibJS runtime into the same process.
function(mite_add_runtime target root js_target)
    find_package(libuv CONFIG REQUIRED)
    find_package(llhttp CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(ZLIB 1.3.2 EXACT REQUIRED)
    find_package(ICU 78.3 EXACT REQUIRED COMPONENTS uc)
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-generated")
    file(READ "${root}/vendor/ladybird.json" upstream_manifest)
    string(JSON MITE_LADYBIRD_REVISION GET "${upstream_manifest}" revision)
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS "${root}/vendor/ladybird.json"
                 "${root}/tooling/check_upstream.py")
    configure_file("${root}/mite/runtime/core/UpstreamVersion.h.in"
                   "${generated}/UpstreamVersion.h" @ONLY)
    file(READ "${root}/mite/modules/generated/bootstrap.js" MITE_BOOTSTRAP_SOURCE)
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS "${root}/mite/modules/generated/bootstrap.js")
    configure_file("${root}/mite/runtime/bootstrap/bootstrap.h.in" "${generated}/MiteBootstrap.h"
                   @ONLY)
    foreach(
        module
        perf_hooks
        events
        process
        process_events
        path
        module
        buffer
        fs
        url
        util
        net
        dns
        http
        https
        fetch
        decompression
        tls
        os
        crypto
        zlib
        child_process
        timers_promises)
        string(TOUPPER "${module}" module_upper)
        file(READ "${root}/mite/modules/generated/${module}.js" "MITE_${module_upper}_SOURCE")
        set_property(
            DIRECTORY
            APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS "${root}/mite/modules/generated/${module}.js")
    endforeach()
    file(READ "${root}/mite/modules/generated/web-globals.js" MITE_WEB_GLOBALS_SOURCE)
    file(READ "${root}/mite/modules/generated/fetch-data.js" MITE_FETCH_DATA_SOURCE)
    file(READ "${root}/mite/modules/generated/stream.js" MITE_STREAM_SOURCE)
    file(READ "${root}/mite/modules/generated/string-decoder.js" MITE_STRING_DECODER_SOURCE)
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${root}/mite/modules/generated/web-globals.js"
                 "${root}/mite/modules/generated/fetch-data.js"
                 "${root}/mite/modules/generated/stream.js"
                 "${root}/mite/modules/generated/string-decoder.js")
    configure_file("${root}/mite/runtime/modules/BuiltinSources.h.in"
                   "${generated}/BuiltinSources.h" @ONLY)
    # Explicit sources keep subsystem ownership visible and builds reproducible.
    set(sources
        # core
        core/Runtime
        core/EventLoop
        core/ImmediateQueue
        # modules
        modules/Builtins
        modules/HostModules
        modules/ModuleLoader
        modules/ModuleUtilities
        modules/ResolveHooks
        modules/ModulePaths
        modules/PackageResolver
        # platform/process
        platform/process/Process
        platform/process/Timing
        platform/process/Signals
        platform/process/ChildProcess
        # platform/filesystem
        platform/filesystem/FileSystem
        # platform/network
        platform/network/Network
        platform/network/DNS
        platform/network/HTTP
        platform/network/tls/Client
        platform/network/tls/TrustStore
        # platform/text
        platform/text/URL
        platform/text/Encoding
        # platform/system
        platform/system/OS
        platform/system/OSConstants
        # platform/compression
        platform/compression/Inflater
        platform/compression/Codec
        # platform/crypto
        platform/crypto/Crypto
        platform/crypto/CryptoHash
        platform/crypto/CryptoHmac)
    set(files)
    foreach(source IN LISTS sources)
        list(APPEND files "${root}/mite/runtime/${source}.cpp")
    endforeach()
    add_library(${target} STATIC ${files})
    target_include_directories(
        ${target}
        PUBLIC "${root}/mite/runtime"
        PRIVATE "${generated}")
    target_link_libraries(
        ${target}
        PUBLIC ${js_target}
               AK
               LibCore
               LibGC
               LibURL
               LibTextCodec
               ICU::uc
               $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
               llhttp::llhttp_static
               OpenSSL::Crypto
               OpenSSL::SSL
               ZLIB::ZLIB)
    if(APPLE)
        target_link_libraries(${target} PUBLIC "-framework Security" "-framework CoreFoundation")
    endif()
    add_custom_target(
        ${target}-integrity
        COMMAND "${Python3_EXECUTABLE}" "${root}/tooling/check_upstream.py"
        COMMAND "${Python3_EXECUTABLE}" "${root}/tooling/check_runtime_js.py"
        VERBATIM)
    add_dependencies(${target} ${target}-integrity)
endfunction()
