# A component launcher is not proof that every helper role has native confinement.
function(firefly_require_windows_runtime)
    if(NOT WIN32)
        return()
    endif()
    if(NOT FIREFLY_EXPERIMENTAL_WINDOWS_RUNTIME)
        message(
            FATAL_ERROR "Windows desktop requires explicit FIREFLY_EXPERIMENTAL_WINDOWS_RUNTIME")
    endif()
    if(NOT TARGET firefly-windows-helper-launch)
        message(FATAL_ERROR "Windows desktop requires the owned native helper confinement launcher")
    endif()
    get_target_property(firefly_windows_roles_complete firefly-windows-helper-launch
                        FIREFLY_WINDOWS_HELPER_ROLES_COMPLETE)
    if(NOT firefly_windows_roles_complete)
        message(
            FATAL_ERROR "Windows desktop remains closed: native helper confinement roles incomplete"
        )
    endif()
endfunction()
