# Select the checked source composition before upstream creates any Cargo commands. Rust still
# receives the original manifest/lock/dependency graph and real compiler.
set(rust_origin_directory "${CMAKE_BINARY_DIR}/firefly-adapters/rust-origin")
set(rust_origin_driver "${rust_origin_directory}/firefly-rustc-driver${CMAKE_EXECUTABLE_SUFFIX}")
if(DEFINED RUSTC_WRAPPER AND NOT RUSTC_WRAPPER STREQUAL rust_origin_driver)
    set(FIREFLY_RUST_CHAIN_WRAPPER
        "${RUSTC_WRAPPER}"
        CACHE FILEPATH "Compiler cache after owned Rust source selection" FORCE)
elseif(NOT DEFINED FIREFLY_RUST_CHAIN_WRAPPER)
    set(FIREFLY_RUST_CHAIN_WRAPPER
        "$ENV{RUSTC_WRAPPER}"
        CACHE FILEPATH "Compiler cache after owned Rust source selection")
endif()
if(NOT "$ENV{RUSTC_WORKSPACE_WRAPPER}" STREQUAL "")
    message(
        FATAL_ERROR
            "The owned Rust origin composition does not support an additional workspace compiler wrapper"
    )
endif()
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/rust-origin/prepare.py" --output
        "${rust_origin_directory}" --driver "${rust_origin_driver}" --chain
        "${FIREFLY_RUST_CHAIN_WRAPPER}"
    RESULT_VARIABLE rust_origin_result)
if(NOT rust_origin_result EQUAL 0)
    message(FATAL_ERROR "Review the pinned Rust origin source composition")
endif()
file(SHA256 "${rust_origin_directory}/lib.rs" FIREFLY_RUST_ORIGIN_FINGERPRINT)
set(rust_driver_source "${FIREFLY_SOURCE_DIR}/tooling/rust-origin/Driver.cpp")
file(SHA256 "${rust_driver_source}" rust_driver_source_hash)
file(SHA256 "${rust_origin_directory}/DriverConfig.h" rust_driver_config_hash)
file(SHA256 "${rust_origin_directory}/config.json" rust_driver_policy_hash)
set(rust_driver_fingerprint
    "${CMAKE_CXX_COMPILER}:${rust_driver_source_hash}:${rust_driver_config_hash}:${rust_driver_policy_hash}"
)
set(rust_driver_previous "")
if(EXISTS "${rust_origin_directory}/driver-fingerprint")
    file(READ "${rust_origin_directory}/driver-fingerprint" rust_driver_previous)
endif()
if(NOT EXISTS "${rust_origin_driver}" OR NOT rust_driver_previous STREQUAL rust_driver_fingerprint)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        execute_process(
            COMMAND
                "${CMAKE_CXX_COMPILER}" /nologo /std:c++17 /EHsc /utf-8 "/I${rust_origin_directory}"
                "${rust_driver_source}" "/Fe${rust_origin_driver}"
                "/Fo${rust_origin_directory}/driver.obj"
            RESULT_VARIABLE rust_driver_result)
    else()
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" -std=c++17 -O2 "-I${rust_origin_directory}"
                    "${rust_driver_source}" -o "${rust_origin_driver}"
            RESULT_VARIABLE rust_driver_result)
    endif()
    if(NOT rust_driver_result EQUAL 0)
        message(FATAL_ERROR "Cannot compile the native build-only Rust source driver")
    endif()
    file(WRITE "${rust_origin_directory}/driver-fingerprint" "${rust_driver_fingerprint}")
endif()
set(RUSTC_WRAPPER
    "${rust_origin_driver}"
    CACHE FILEPATH "Owned checked Rust origin source driver" FORCE)
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/rust-origin/prepare.py"
             "${FIREFLY_SOURCE_DIR}/tooling/rust-origin/driver.py"
             "${rust_driver_source}"
             "${FIREFLY_SOURCE_DIR}/firefly/native/protocol/rust/origin_policy.rs"
             "${FIREFLY_SOURCE_DIR}/vendor/ladybird/Libraries/LibURL/Rust/src/lib.rs"
             "${FIREFLY_SOURCE_DIR}/vendor/ladybird/Libraries/LibURL/Rust/src/origin.rs"
             "${FIREFLY_SOURCE_DIR}/vendor/ladybird/Libraries/LibURL/Rust/src/ffi.rs")
