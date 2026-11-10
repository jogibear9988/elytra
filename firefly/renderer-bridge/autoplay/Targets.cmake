# Native per-context media/WebAudio enforcement and bounded public preference values.
include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
set(autoplay_output "${CMAKE_BINARY_DIR}/firefly-adapters/autoplay")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_autoplay_media.py"
            --output-dir "${autoplay_output}" RESULT_VARIABLE autoplay_check)
if(NOT autoplay_check EQUAL 0)
    message(FATAL_ERROR "Review native autoplay media source composition")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_autoplay_media.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/HTMLMediaElement.cpp")
firefly_replace_source(LibWeb "Libraries/LibWeb/HTML/HTMLMediaElement.cpp"
                       "${autoplay_output}/HTMLMediaElement.cpp")
target_sources(
    LibWeb PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Policy.cpp" "${CMAKE_CURRENT_LIST_DIR}/Media.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/Audio.cpp")
set_property(
    SOURCE "${autoplay_output}/HTMLMediaElement.cpp" TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
add_custom_target(
    check-firefly-autoplay-media-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_autoplay_media.py"
            --output-dir "${autoplay_output}"
    COMMAND "${Python3_EXECUTABLE}"
            "${FIREFLY_SOURCE_DIR}/tooling/tests/test_autoplay_media_adapter.py"
    VERBATIM)
add_dependencies(LibWeb check-firefly-autoplay-media-hooks)

# Validate the real compiler route as well as the generated source selection.
add_custom_target(
    check-firefly-autoplay-rust-routing
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/rust-autoplay/prepare.py" --output
        "${autoplay_directory}" --driver "${autoplay_driver}" --chain
        "${FIREFLY_AUTOPLAY_CHAIN_WRAPPER}" --check
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/rust-autoplay/test_audio.py"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/rust-autoplay/test_driver.py"
            --compiler "${CMAKE_CXX_COMPILER}"
    DEPENDS libweb_bindings_rust-build
    VERBATIM)
