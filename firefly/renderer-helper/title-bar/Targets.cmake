# Use the existing media snapshot as per-document style input, including on worker threads.
include("${FIREFLY_SOURCE_DIR}/firefly/renderer/SourceAdapter.cmake")
firefly_replace_source(LibWeb "Libraries/LibWeb/CSS/MediaList.cpp"
                       "${title_bar_css_output}/MediaList.cpp")
# DOM callback allocation is private to LibWeb; keep it in the owning library.
target_sources(LibWeb PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Overlay.cpp")
