# Only the renderer graph composes this policy; standalone Mite stays untouched.
set(protocol_origin_output "${CMAKE_BINARY_DIR}/firefly-adapters/protocol-origins")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_protocol_origin_hooks.py"
            --output "${protocol_origin_output}" RESULT_VARIABLE protocol_origin_check)
if(NOT protocol_origin_check EQUAL 0)
    message(FATAL_ERROR "Review the owned protocol origin adapters")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/prepare_protocol_origin_hooks.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebView/HelperProcess.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebCommon/Fetch/Infrastructure/URL.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWebCommon/SecureContexts/AbstractOperations.cpp")

function(firefly_origin_source target original replacement)
    get_target_property(sources "${target}" SOURCES)
    get_target_property(source_directory "${target}" SOURCE_DIR)
    set(adapted)
    set(count 0)
    foreach(source IN LISTS sources)
        get_filename_component(absolute "${source}" ABSOLUTE BASE_DIR "${source_directory}")
        if(absolute STREQUAL "${LADYBIRD_SOURCE_DIR}/${original}")
            list(APPEND adapted "${protocol_origin_output}/${replacement}")
            math(EXPR count "${count}+1")
        else()
            list(APPEND adapted "${source}")
        endif()
    endforeach()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Review ${target} origin source layout")
    endif()
    set_property(TARGET "${target}" PROPERTY SOURCES "${adapted}")
    set_property(
        SOURCE "${protocol_origin_output}/${replacement}" TARGET_DIRECTORY "${target}"
        APPEND
        PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly")
endfunction()
firefly_origin_source(LibWebCommon "Libraries/LibWebCommon/SecureContexts/AbstractOperations.cpp"
                      SecureContexts.cpp)
firefly_origin_source(LibWebCommon "Libraries/LibWebCommon/Fetch/Infrastructure/URL.cpp"
                      FetchURL.cpp)
firefly_origin_source(LibWebView "Libraries/LibWebView/HelperProcess.cpp" HelperProcess.cpp)
target_sources(
    LibURL
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/OriginPolicy.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/OriginConfig.cpp" "${CMAKE_CURRENT_LIST_DIR}/RustOrigin.cpp")
add_custom_target(
    check-firefly-rust-origin
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/rust-origin/prepare.py" --output
        "${rust_origin_directory}" --driver "${rust_origin_driver}" --chain
        "${FIREFLY_RUST_CHAIN_WRAPPER}" --check
    VERBATIM)
add_dependencies(LibURL check-firefly-rust-origin)
add_dependencies(liburl_rust-build check-firefly-rust-origin)
add_dependencies(libweb_bindings_rust-build check-firefly-rust-origin)
get_target_property(rust_url_archive liburl_rust IMPORTED_LOCATION)
get_target_property(rust_web_archive libweb_bindings_rust IMPORTED_LOCATION)
get_filename_component(rust_url_directory "${rust_url_archive}" DIRECTORY)
get_filename_component(rust_url_name "${rust_url_archive}" NAME_WE)
get_filename_component(rust_web_directory "${rust_web_archive}" DIRECTORY)
get_filename_component(rust_web_name "${rust_web_archive}" NAME_WE)
add_custom_target(
    check-firefly-rust-origin-dependencies
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/rust-origin/audit.py" --config
        "${rust_origin_directory}/config.json" --depfile "${rust_url_directory}/${rust_url_name}.d"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/rust-origin/test_driver.py"
            --compiler "${CMAKE_CXX_COMPILER}"
    DEPENDS liburl_rust-build
    VERBATIM)
# Keep the LibURL-only Windows gate independent of broader LibWeb bindings.
add_custom_target(
    check-firefly-rust-dom-origin-dependencies
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/rust-origin/audit.py" --config
        "${rust_origin_directory}/config.json" --depfile "${rust_web_directory}/${rust_web_name}.d"
    DEPENDS check-firefly-rust-origin-dependencies libweb_bindings_rust-build
    VERBATIM)
add_custom_target(
    check-firefly-protocol-origin-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_protocol_origin_hooks.py"
            --output "${protocol_origin_output}"
    VERBATIM)
add_dependencies(LibURL check-firefly-protocol-origin-hooks)
add_dependencies(LibWebCommon check-firefly-protocol-origin-hooks)
add_dependencies(LibWebView check-firefly-protocol-origin-hooks)
add_executable(firefly-protocol-origin-probe "${CMAKE_CURRENT_LIST_DIR}/OriginProbe.cpp")
target_include_directories(firefly-protocol-origin-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-protocol-origin-probe PRIVATE LibURL LibWebCommon LibMain LibCore)
ladybird_copy_runtime_dlls(firefly-protocol-origin-probe)
add_executable(firefly-protocol-routing-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/protocol/RoutingProbe.cpp")
target_include_directories(firefly-protocol-routing-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-protocol-routing-probe PRIVATE LibWebView LibWebCommon LibURL LibMain
                                                             LibCore)
ladybird_check_skia_confinement(firefly-protocol-routing-probe)
ladybird_copy_runtime_dlls(firefly-protocol-routing-probe)
add_custom_target(
    check-firefly-protocol-routing
    COMMAND "$<TARGET_FILE:firefly-protocol-routing-probe>"
    DEPENDS firefly-protocol-routing-probe
    VERBATIM)
add_custom_target(
    check-firefly-protocol-origins
    COMMAND "$<TARGET_FILE:firefly-protocol-origin-probe>" default
    COMMAND "$<TARGET_FILE:firefly-protocol-origin-probe>" configured
    COMMAND "$<TARGET_FILE:firefly-protocol-origin-probe>" arguments
    COMMAND "$<TARGET_FILE:firefly-protocol-origin-probe>" sealed
    DEPENDS firefly-protocol-origin-probe check-firefly-rust-origin-dependencies
    USES_TERMINAL VERBATIM)
