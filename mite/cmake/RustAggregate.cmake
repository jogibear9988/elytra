# Standalone Mite needs one Cargo feature graph. Independently linked staticlibs
# each carry shared dependencies (and potentially different Unicode features).
# Keep upstream's FFI generation and initializers, but link one owned archive.
function(mite_aggregate_rust)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tooling/check_mite_rust_lock.py"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${output}${error}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/mite/rust/Cargo.lock"
        "${CMAKE_SOURCE_DIR}/vendor/ladybird/Cargo.lock"
        "${CMAKE_SOURCE_DIR}/tooling/check_mite_rust_lock.py")
    _rust_crate_common_setup(MANIFEST_PATH mite/rust/Cargo.toml CRATE_NAME mite_runtime_rust)
    set(cargo_feature_flags --locked)
    foreach(feature IN LISTS LIBJS_RUST_FEATURES)
        list(APPEND cargo_feature_flags "--features=libjs_rust/${feature}")
    endforeach()
    if(WIN32)
        set(archive "${cargo_output_dir}/mite_runtime_rust.lib")
        set(depfile "${cargo_output_dir}/mite_runtime_rust.d")
    else()
        set(archive "${cargo_output_dir}/libmite_runtime_rust.a")
        set(depfile "${cargo_output_dir}/libmite_runtime_rust.d")
    endif()
    _rust_crate_build_command(OUTPUT "${archive}" DEPFILE "${depfile}")
    add_custom_target(mite-rust-build DEPENDS "${archive}")
    foreach(crate libjs_rust liburl_rust libunicode_rust libregex_rust libtextcodec_rust)
        foreach(target "${crate}" "${crate}_archive")
            if(TARGET "${target}")
                set_property(TARGET "${target}" PROPERTY IMPORTED_LOCATION "${archive}")
                add_dependencies("${target}" mite-rust-build)
            endif()
        endforeach()
    endforeach()
    # With a single graph, static ELF links must also reject duplicate definitions.
    foreach(target libjs_rust LibRegex)
        get_target_property(options "${target}" INTERFACE_LINK_OPTIONS)
        if(options)
            list(REMOVE_ITEM options "LINKER:--allow-multiple-definition")
            set_property(TARGET "${target}" PROPERTY INTERFACE_LINK_OPTIONS "${options}")
        endif()
    endforeach()
endfunction()
