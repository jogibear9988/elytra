# The pinned Qt headers expose Wayland native interfaces only with this public feature. Inspect the
# actual imported SDK, not just our requested vcpkg feature list.
function(firefly_require_wayland_native_interfaces)
    get_target_property(features Qt::Gui QT_ENABLED_PUBLIC_FEATURES)
    if(NOT "wayland" IN_LIST features)
        message(
            FATAL_ERROR
                "Pinned Linux Ladybird requires Qt Wayland native interfaces; regenerate the owned Qt manifest/provider"
        )
    endif()
endfunction()
