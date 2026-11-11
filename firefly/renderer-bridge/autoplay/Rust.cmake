# Route genuine LibWeb WebAudio Rust leaves before upstream emits Cargo commands. Public autoplay
# options remain rejected until renderer acceptance is complete.
include_guard(GLOBAL)
if(NOT FIREFLY_RUST_NOJIT_FINGERPRINT)
    message(FATAL_ERROR "Include WebAudio Rust routing after the existing non-JIT/origin chain")
endif()
get_filename_component(FIREFLY_AUTOPLAY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(autoplay_directory "${CMAKE_BINARY_DIR}/firefly-adapters/rust-autoplay")
set(autoplay_driver "${autoplay_directory}/firefly-autoplay-rustc-driver${CMAKE_EXECUTABLE_SUFFIX}")
if(DEFINED RUSTC_WRAPPER AND NOT RUSTC_WRAPPER STREQUAL autoplay_driver)
    set(FIREFLY_AUTOPLAY_CHAIN_WRAPPER
        "${RUSTC_WRAPPER}"
        CACHE FILEPATH "Existing non-JIT/origin/compiler-cache chain" FORCE)
elseif(NOT DEFINED FIREFLY_AUTOPLAY_CHAIN_WRAPPER)
    message(FATAL_ERROR "Missing previous Rust source-selector chain")
endif()
if(NOT "$ENV{RUSTC_WORKSPACE_WRAPPER}" STREQUAL "")
    message(
        FATAL_ERROR "WebAudio source routing does not support another workspace compiler wrapper")
endif()
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_AUTOPLAY_ROOT}/tooling/rust-autoplay/prepare.py" --output
        "${autoplay_directory}" --driver "${autoplay_driver}" --chain
        "${FIREFLY_AUTOPLAY_CHAIN_WRAPPER}"
    RESULT_VARIABLE autoplay_result)
if(NOT autoplay_result EQUAL 0)
    message(FATAL_ERROR "Review pinned WebAudio Rust source routing")
endif()
file(SHA256 "${autoplay_directory}/lib.rs" FIREFLY_RUST_AUTOPLAY_FINGERPRINT)
set(autoplay_driver_source "${FIREFLY_AUTOPLAY_ROOT}/tooling/rust-autoplay/Driver.cpp")
file(SHA256 "${autoplay_driver_source}" autoplay_driver_source_hash)
file(SHA256 "${autoplay_directory}/DriverConfig.h" autoplay_driver_config_hash)
file(SHA256 "${autoplay_directory}/config.json" autoplay_driver_policy_hash)
set(autoplay_driver_fingerprint
    "${CMAKE_CXX_COMPILER}:${autoplay_driver_source_hash}:${autoplay_driver_config_hash}:${autoplay_driver_policy_hash}"
)
set(autoplay_driver_previous "")
if(EXISTS "${autoplay_directory}/driver-fingerprint")
    file(READ "${autoplay_directory}/driver-fingerprint" autoplay_driver_previous)
endif()
if(NOT EXISTS "${autoplay_driver}" OR NOT autoplay_driver_previous STREQUAL
                                      autoplay_driver_fingerprint)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        execute_process(
            COMMAND
                "${CMAKE_CXX_COMPILER}" /nologo /std:c++17 /EHsc /utf-8 "/I${autoplay_directory}"
                "${autoplay_driver_source}" "/Fe${autoplay_driver}"
                "/Fo${autoplay_directory}/driver.obj"
            RESULT_VARIABLE autoplay_driver_result)
    else()
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" -std=c++17 -O2 "-I${autoplay_directory}"
                    "${autoplay_driver_source}" -o "${autoplay_driver}"
            RESULT_VARIABLE autoplay_driver_result)
    endif()
    if(NOT autoplay_driver_result EQUAL 0)
        message(FATAL_ERROR "Cannot compile the native build-only WebAudio source driver")
    endif()
    file(WRITE "${autoplay_directory}/driver-fingerprint" "${autoplay_driver_fingerprint}")
endif()
# Do not overwrite the cached wrapper: the previous include reconstructs no-JIT first on the next
# configure, avoiding recursive chains of this outer driver.
set(RUSTC_WRAPPER "${autoplay_driver}")
string(APPEND RUSTC_TARGET_FLAGS
       " --cfg=firefly_autoplay_build_${FIREFLY_RUST_AUTOPLAY_FINGERPRINT}")
set_property(
    DIRECTORY
    APPEND
    PROPERTY
        CMAKE_CONFIGURE_DEPENDS
        "${FIREFLY_AUTOPLAY_ROOT}/tooling/rust-ports/observer.py"
        "${FIREFLY_AUTOPLAY_ROOT}/vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src/html/mod.rs"
        "${FIREFLY_AUTOPLAY_ROOT}/vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src/html/message_port.rs"
        "${FIREFLY_AUTOPLAY_ROOT}/vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src/ffi_test_stubs.rs"
        "${FIREFLY_AUTOPLAY_ROOT}/tooling/rust-autoplay/audio.py"
        "${FIREFLY_AUTOPLAY_ROOT}/tooling/rust-autoplay/prepare.py"
        "${FIREFLY_AUTOPLAY_ROOT}/tooling/rust-autoplay/driver.py"
        "${FIREFLY_AUTOPLAY_ROOT}/tooling/rust-autoplay/Driver.cpp"
        "${FIREFLY_AUTOPLAY_ROOT}/vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src/lib.rs"
        "${FIREFLY_AUTOPLAY_ROOT}/vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src/web_audio/mod.rs"
        "${FIREFLY_AUTOPLAY_ROOT}/vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src/web_audio/audio_context.rs"
        "${FIREFLY_AUTOPLAY_ROOT}/vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src/web_audio/audio_scheduled_source_node.rs"
        "${FIREFLY_AUTOPLAY_ROOT}/vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src/web_audio/audio_buffer_source_node.rs"
)
