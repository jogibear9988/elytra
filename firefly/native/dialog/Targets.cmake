# Privileged main-process dialogs are distinct from renderer file-input snapshots.
add_library(firefly-dialog STATIC "${CMAKE_CURRENT_LIST_DIR}/Provider.cpp")
target_link_libraries(firefly-dialog PUBLIC Qt::Core Qt::Widgets)
if(APPLE)
    target_sources(firefly-dialog PRIVATE "${CMAKE_CURRENT_LIST_DIR}/FilesMac.mm")
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/FilesMac.mm" PROPERTIES COMPILE_OPTIONS
                                                                                   "-fobjc-arc")
    target_link_libraries(firefly-dialog PUBLIC "-framework AppKit"
                                                "-framework UniformTypeIdentifiers")
else()
    target_sources(firefly-dialog PRIVATE "${CMAKE_CURRENT_LIST_DIR}/FilesUnsupported.cpp")
endif()
