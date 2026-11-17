# Native version requirements follow the same reviewed manifest as dependency installation.
include_guard(GLOBAL)

function(mite_dependency_version root name output)
    set(manifest "${root}/vcpkg.json")
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
    file(READ "${manifest}" contents)
    string(JSON count LENGTH "${contents}" overrides)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(
            JSON
            candidate
            GET
            "${contents}"
            overrides
            ${index}
            name)
        if(candidate STREQUAL name)
            string(
                JSON
                version
                GET
                "${contents}"
                overrides
                ${index}
                version)
            # A vcpkg port revision is not part of the library's native version.
            string(REGEX REPLACE "#[0-9]+$" "" version "${version}")
            if(NOT version MATCHES "^[0-9]+\\.[0-9]+(\\.[0-9]+)?$")
                message(FATAL_ERROR "Invalid native version pin for ${name}: ${version}")
            endif()
            set(${output}
                "${version}"
                PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "Missing explicit native version pin for ${name}")
endfunction()
