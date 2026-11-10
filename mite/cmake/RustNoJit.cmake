# Select the checked source composition before upstream creates any Cargo commands. Rust still
# receives the original manifest/lock/dependency graph and real compiler.
get_filename_component(MITE_RUST_ADAPTER_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(mite_nojit_directory "${CMAKE_BINARY_DIR}/mite-adapters/rust-nojit")
set(mite_nojit_driver "${mite_nojit_directory}/mite-nojit-rustc-driver${CMAKE_EXECUTABLE_SUFFIX}")
if(DEFINED RUSTC_WRAPPER AND NOT RUSTC_WRAPPER STREQUAL mite_nojit_driver)
    set(MITE_RUST_CHAIN_WRAPPER
        "${RUSTC_WRAPPER}"
        CACHE FILEPATH "Compiler cache after owned Rust source selection" FORCE)
elseif(NOT DEFINED MITE_RUST_CHAIN_WRAPPER)
    set(MITE_RUST_CHAIN_WRAPPER
        "$ENV{RUSTC_WRAPPER}"
        CACHE FILEPATH "Compiler cache after owned Rust source selection")
endif()
if(NOT "$ENV{RUSTC_WORKSPACE_WRAPPER}" STREQUAL "")
    message(
        FATAL_ERROR
            "The owned Rust non-JIT composition does not support an additional workspace compiler wrapper"
    )
endif()
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" "${MITE_RUST_ADAPTER_ROOT}/tooling/rust-nojit/prepare.py" --output
        "${mite_nojit_directory}" --driver "${mite_nojit_driver}" --chain
        "${MITE_RUST_CHAIN_WRAPPER}"
    RESULT_VARIABLE mite_nojit_result)
if(NOT mite_nojit_result EQUAL 0)
    message(FATAL_ERROR "Review the pinned Rust non-JIT source composition")
endif()
file(SHA256 "${mite_nojit_directory}/lib.rs" FIREFLY_RUST_NOJIT_FINGERPRINT)
set(rust_driver_source "${MITE_RUST_ADAPTER_ROOT}/tooling/rust-nojit/Driver.cpp")
file(SHA256 "${rust_driver_source}" rust_driver_source_hash)
file(SHA256 "${mite_nojit_directory}/DriverConfig.h" rust_driver_config_hash)
file(SHA256 "${mite_nojit_directory}/config.json" rust_driver_policy_hash)
set(rust_driver_fingerprint
    "${CMAKE_CXX_COMPILER}:${rust_driver_source_hash}:${rust_driver_config_hash}:${rust_driver_policy_hash}"
)
set(rust_driver_previous "")
if(EXISTS "${mite_nojit_directory}/driver-fingerprint")
    file(READ "${mite_nojit_directory}/driver-fingerprint" rust_driver_previous)
endif()
if(NOT EXISTS "${mite_nojit_driver}" OR NOT rust_driver_previous STREQUAL rust_driver_fingerprint)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        execute_process(
            COMMAND
                "${CMAKE_CXX_COMPILER}" /nologo /std:c++17 /EHsc /utf-8 "/I${mite_nojit_directory}"
                "${rust_driver_source}" "/Fe${mite_nojit_driver}"
                "/Fo${mite_nojit_directory}/driver.obj"
            RESULT_VARIABLE rust_driver_result)
    else()
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" -std=c++17 -O2 "-I${mite_nojit_directory}"
                    "${rust_driver_source}" -o "${mite_nojit_driver}"
            RESULT_VARIABLE rust_driver_result)
    endif()
    if(NOT rust_driver_result EQUAL 0)
        message(FATAL_ERROR "Cannot compile the native build-only Rust source driver")
    endif()
    file(WRITE "${mite_nojit_directory}/driver-fingerprint" "${rust_driver_fingerprint}")
endif()
# Keep the persistent cache wrapper unchanged. In the renderer it is the owned origin driver;
# caching this outer driver would make the next configure chain the two drivers recursively into one
# another.
set(RUSTC_WRAPPER "${mite_nojit_driver}")
string(APPEND RUSTC_TARGET_FLAGS " --cfg=mite_nojit_build_${FIREFLY_RUST_NOJIT_FINGERPRINT}")
set_property(
    DIRECTORY
    APPEND
    PROPERTY
        CMAKE_CONFIGURE_DEPENDS
        "${MITE_RUST_ADAPTER_ROOT}/tooling/rust-nojit/prepare.py"
        "${MITE_RUST_ADAPTER_ROOT}/tooling/rust-nojit/driver.py"
        "${MITE_RUST_ADAPTER_ROOT}/tooling/rust-nojit/Driver.cpp"
        "${MITE_RUST_ADAPTER_ROOT}/vendor/ladybird/Libraries/LibJS/Rust/src/lib.rs"
        "${MITE_RUST_ADAPTER_ROOT}/vendor/ladybird/Libraries/LibJS/Rust/src/unicode.rs"
        "${MITE_RUST_ADAPTER_ROOT}/vendor/ladybird/Libraries/LibJS/Rust/src/runtime/mod.rs"
        "${MITE_RUST_ADAPTER_ROOT}/vendor/ladybird/Libraries/LibJS/Rust/src/runtime/ecmascript_function_object.rs"
)
