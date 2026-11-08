# Native macOS presentation/accessibility sources from the pinned upstream tree.
foreach(
    source
    Conversions
    DictionaryLookup
    LadybirdAccessibilityElement
    MacWindow
    MacWindowFocus
    NativeTextEditingBindings
    SystemAppearanceMac
    WebContentViewMac)
    target_sources(firefly-qt-view PRIVATE "${LADYBIRD_SOURCE_DIR}/UI/Qt/${source}.mm")
endforeach()
# The native AX bridge casts Qt winId to NSView. Non-Cocoa test platforms have synthetic handles
# instead; guard the owned generated copy without vendor edits.
set(prepared_accessibility "${CMAKE_BINARY_DIR}/firefly-generated/WebContentViewAccessibility.mm")
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_accessibility_hook.py"
        --output "${prepared_accessibility}"
    RESULT_VARIABLE accessibility_check)
if(NOT accessibility_check EQUAL 0)
    message(FATAL_ERROR "Review owned Cocoa accessibility platform adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_accessibility_hook.py"
             "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentViewAccessibility.mm"
             "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentViewAccessibility.h")
target_sources(firefly-qt-view PRIVATE "${prepared_accessibility}")
add_custom_target(
    check-firefly-view-accessibility
    COMMAND
        "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_view_accessibility_hook.py"
        --output "${prepared_accessibility}"
    VERBATIM)
add_dependencies(firefly-qt-view check-firefly-view-accessibility)
set_source_files_properties("${LADYBIRD_SOURCE_DIR}/UI/Qt/LadybirdAccessibilityElement.mm"
                            "${prepared_accessibility}" PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_link_libraries(firefly-qt-view PUBLIC "-framework ApplicationServices" "-framework Cocoa"
                                             "-framework IOSurface" "-framework QuartzCore")
