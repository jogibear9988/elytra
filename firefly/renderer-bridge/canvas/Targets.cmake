# Compose both real canvas consumers without changing the pinned vendor tree.
include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
set(canvas_output "${CMAKE_BINARY_DIR}/firefly-adapters/canvas")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_canvas_hooks.py" --output
            "${canvas_output}" RESULT_VARIABLE canvas_check)
if(NOT canvas_check EQUAL 0)
    message(FATAL_ERROR "Review renderer canvas background adapters")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_canvas_hooks.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/DOM/Document.cpp"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/Painting/PaintingRustBridge.cpp")
firefly_replace_source(LibWeb "Libraries/LibWeb/DOM/Document.cpp" "${canvas_output}/Document.cpp")
firefly_replace_source(LibWeb "Libraries/LibWeb/Painting/PaintingRustBridge.cpp"
                       "${canvas_output}/PaintingRustBridge.cpp")
target_sources(LibWeb PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Canvas.cpp")
set_property(
    SOURCE "${canvas_output}/Document.cpp" "${canvas_output}/PaintingRustBridge.cpp"
           TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
add_custom_target(
    check-firefly-canvas-hooks
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_canvas_hooks.py" --output
            "${canvas_output}"
    VERBATIM)
add_dependencies(LibWeb check-firefly-canvas-hooks)
