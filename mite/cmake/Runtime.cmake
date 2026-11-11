# One runtime source/generation graph for the standalone CLI and native hosts. The caller chooses
# the upstream JS target: a host already using LibWeb must not accidentally link a second standalone
# LibJS runtime into the same process.
function(mite_add_runtime target root js_target)
    find_package(libuv CONFIG REQUIRED)
    find_package(llhttp CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(SQLite3 3.53.4 EXACT REQUIRED)
    if(TARGET SQLite3::SQLite3)
        set(mite_sqlite_target SQLite3::SQLite3)
    else()
        # CMake 3.30's FindSQLite3 exports only the legacy target name.
        set(mite_sqlite_target SQLite::SQLite3)
    endif()
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
        diagnostics_channel
        assert
        process
        process_events
        process_stdio
        policy-watcher
        sqlite
        native-keymap
        path
        module
        buffer
        fs
        url
        querystring
        async_hooks
        util
        util-types
        console
        net
        dns
        http
        https
        fetch
        decompression
        tls
        os
        inspector
        crypto
        zlib
        readline
        child_process
        worker_threads
        tty
        cjs-lexer
        timers_promises)
        string(TOUPPER "${module}" module_upper)
        string(REPLACE "-" "_" module_upper "${module_upper}")
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
        core/workers/Host
        core/async/Context
        core/errors/StackTrace
        core/errors/CallSites
        # modules
        modules/Builtins
        modules/PolicyWatcher
        modules/StorageAndKeyboard
        modules/HostModules
        modules/ModuleLoader
        modules/CommonJSExports
        modules/ModuleUtilities
        modules/ResolveHooks
        modules/ModulePaths
        modules/PackageResolver
        # platform/process
        platform/process/Process
        platform/process/Exit
        platform/process/Timing
        platform/process/Signals
        # platform/terminal
        platform/terminal/TTY
        platform/process/ChildProcess
        # platform/filesystem
        platform/filesystem/FileSystem
        platform/filesystem/Clone
        platform/filesystem/Directory
        platform/filesystem/StatWatcher
        platform/filesystem/EventWatcher
        # platform/network
        platform/network/Network
        platform/network/DNS
        platform/network/HTTP
        platform/network/tls/Client
        platform/network/tls/TrustStore
        platform/network/tls/ExtraCertificates
        # platform/text
        platform/text/URL
        platform/text/Encoding
        # platform/system
        platform/system/OS
        platform/system/Inspector
        platform/util/Types
        platform/system/OSConstants
        # platform/policy
        platform/policy/Host
        platform/keyboard/Host
        storage/sqlite/Host
        storage/sqlite/Store
        # platform/compression
        platform/compression/Inflater
        platform/compression/Codec
        # platform/crypto
        platform/crypto/Crypto
        platform/crypto/CryptoHash
        platform/crypto/CryptoHmac
        platform/crypto/CryptoX509
        platform/crypto/CryptoVerify)
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
               ZLIB::ZLIB
               ${mite_sqlite_target})
    if(APPLE)
        target_sources(${target} PRIVATE "${root}/mite/runtime/platform/policy/ProviderMac.cpp"
                                         "${root}/mite/runtime/platform/keyboard/ProviderMac.cpp")
        target_link_libraries(${target} PUBLIC "-framework Carbon")
        file(SHA256 "${root}/vendor/native-keymap/deps/chromium/dom_code_data.inc"
             native_keymap_hash)
        if(NOT native_keymap_hash STREQUAL
           "c5d1825155d67badfd3f344e81988f323a656e2acc78dde237faeb830b707660")
            message(FATAL_ERROR "Review pinned native-keymap physical key table")
        endif()
        target_link_libraries(${target} PUBLIC "-framework Security" "-framework CoreFoundation"
                                               "-framework CoreServices")
    else()
        target_sources(
            ${target} PRIVATE "${root}/mite/runtime/platform/policy/ProviderUnsupported.cpp"
                              "${root}/mite/runtime/platform/keyboard/ProviderUnsupported.cpp")
    endif()
    add_custom_target(
        ${target}-integrity
        COMMAND "${Python3_EXECUTABLE}" "${root}/tooling/check_upstream.py"
        COMMAND "${Python3_EXECUTABLE}" "${root}/tooling/check_runtime_js.py"
        VERBATIM)
    add_dependencies(${target} ${target}-integrity)
endfunction()
