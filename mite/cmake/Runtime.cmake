# One runtime source/generation graph for the standalone CLI and native hosts.
# The caller chooses the upstream JS target: a host already using LibWeb must not
# accidentally link a second standalone LibJS runtime into the same process.
function(mite_add_runtime target root js_target)
    find_package(libuv CONFIG REQUIRED)
    find_package(llhttp CONFIG REQUIRED)
    find_package(ICU 78.3 EXACT REQUIRED COMPONENTS uc)
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-generated")
    file(READ "${root}/vendor/ladybird.json" upstream_manifest)
    string(JSON MITE_LADYBIRD_REVISION GET "${upstream_manifest}" revision)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${root}/vendor/ladybird.json" "${root}/tooling/check_upstream.py")
    configure_file("${root}/mite/runtime/UpstreamVersion.h.in" "${generated}/UpstreamVersion.h" @ONLY)
    file(READ "${root}/mite/runtime/bootstrap.js" MITE_BOOTSTRAP_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${root}/mite/runtime/bootstrap.js")
    configure_file("${root}/mite/runtime/bootstrap.h.in" "${generated}/MiteBootstrap.h" @ONLY)
    foreach(module events process path buffer fs url util net dns http os)
        string(TOUPPER "${module}" module_upper)
        file(READ "${root}/mite/modules/${module}.js" "MITE_${module_upper}_SOURCE")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${root}/mite/modules/${module}.js")
    endforeach()
    file(READ "${root}/mite/modules/generated/stream.js" MITE_STREAM_SOURCE)
    file(READ "${root}/mite/modules/generated/string-decoder.js" MITE_STRING_DECODER_SOURCE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${root}/mite/modules/generated/stream.js" "${root}/mite/modules/generated/string-decoder.js")
    configure_file("${root}/mite/runtime/BuiltinSources.h.in" "${generated}/BuiltinSources.h" @ONLY)
    set(sources Runtime EventLoop ModuleLoader PackageResolver HostModules Builtins Process FileSystem URL Encoding Network DNS HTTP OS)
    set(files)
    foreach(source IN LISTS sources)
        list(APPEND files "${root}/mite/runtime/${source}.cpp")
    endforeach()
    add_library(${target} STATIC ${files})
    target_include_directories(${target} PUBLIC "${root}/mite/runtime" PRIVATE "${generated}")
    target_link_libraries(${target} PUBLIC ${js_target} AK LibCore LibGC LibURL LibTextCodec ICU::uc
        $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv> llhttp::llhttp_static)
    add_custom_target(${target}-integrity
        COMMAND "${Python3_EXECUTABLE}" "${root}/tooling/check_upstream.py"
        COMMAND "${Python3_EXECUTABLE}" "${root}/tooling/check_runtime_js.py" VERBATIM)
    add_dependencies(${target} ${target}-integrity)
endfunction()
