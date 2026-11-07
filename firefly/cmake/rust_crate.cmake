# Keep upstream Cargo targets intact. An explicit compilation input invalidates Cargo's existing
# artifacts when the owned source composition changes: changing RUSTC_WRAPPER alone does not make
# Cargo rebuild an already fresh dependency.
include("${LADYBIRD_SOURCE_DIR}/Meta/CMake/rust_crate.cmake")
if(NOT FIREFLY_RUST_ORIGIN_FINGERPRINT MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "Missing owned Rust origin build fingerprint")
endif()
string(APPEND RUSTC_TARGET_FLAGS " --cfg=firefly_origin_build_${FIREFLY_RUST_ORIGIN_FINGERPRINT}")
