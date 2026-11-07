# Reuse the pinned native DirectX view. No alternate renderer or offscreen Qt platform.
if(NOT HAS_DIRECTX)
    message(FATAL_ERROR "Experimental native Windows view requires the pinned DirectX backend")
endif()
target_sources(firefly-qt-view PRIVATE "${LADYBIRD_SOURCE_DIR}/UI/Qt/WebContentViewWindows.cpp")
target_link_libraries(firefly-qt-view PRIVATE d3d11)
