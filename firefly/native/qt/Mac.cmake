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
    WebContentViewAccessibility
    WebContentViewMac)
    target_sources(firefly-qt-view PRIVATE "${LADYBIRD_SOURCE_DIR}/UI/Qt/${source}.mm")
endforeach()
set_source_files_properties(
    "${LADYBIRD_SOURCE_DIR}/UI/Qt/LadybirdAccessibilityElement.mm"
    "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentViewAccessibility.mm" PROPERTIES COMPILE_FLAGS
                                                                             "-fobjc-arc")
target_link_libraries(firefly-qt-view PUBLIC "-framework ApplicationServices" "-framework Cocoa"
                                             "-framework IOSurface" "-framework QuartzCore")
