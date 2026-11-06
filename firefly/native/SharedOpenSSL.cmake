# Optional renderer-graph consolidation. Keep the independent Mite graph unchanged.
option(FIREFLY_SHARED_OPENSSL "Use one explicit OpenSSL provider in macOS Firefly apps" OFF)
if(NOT FIREFLY_SHARED_OPENSSL)
    return()
endif()
if(NOT APPLE
   OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64"
   OR NOT BUILD_SHARED_LIBS
   OR NOT FIREFLY_BUILD_RUNTIME
   OR (CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64"))
    message(
        FATAL_ERROR
            "Shared OpenSSL is currently supported only in the macOS ARM64 shared Firefly runtime graph"
    )
endif()
foreach(archive OPENSSL_CRYPTO_LIBRARY OPENSSL_SSL_LIBRARY)
    if(NOT EXISTS "${${archive}}" OR NOT "${${archive}}" MATCHES "\\.a$")
        message(FATAL_ERROR "Shared OpenSSL requires the pinned static ${archive} archive")
    endif()
endforeach()
# Export the complete pinned SSL/Crypto API deliberately, instead of depending on the subset of
# OpenSSL symbols accidentally exported by another engine library.
add_library(firefly-openssl SHARED "${FIREFLY_SOURCE_DIR}/firefly/native/OpenSSLProvider.c")
set_target_properties(
    firefly-openssl
    PROPERTIES OUTPUT_NAME firefly-openssl
               C_VISIBILITY_PRESET default
               OSX_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}")
target_link_libraries(
    firefly-openssl
    PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,${OPENSSL_SSL_LIBRARY},${OPENSSL_CRYPTO_LIBRARY}>")
target_include_directories(firefly-openssl SYSTEM PUBLIC "${OPENSSL_INCLUDE_DIR}")
foreach(original OpenSSL::Crypto OpenSSL::SSL)
    foreach(property INTERFACE_COMPILE_DEFINITIONS INTERFACE_COMPILE_OPTIONS)
        get_target_property(value ${original} ${property})
        if(value)
            set_property(
                TARGET firefly-openssl
                APPEND
                PROPERTY ${property} "${value}")
        endif()
    endforeach()
endforeach()

function(firefly_use_shared_openssl directory)
    get_property(
        targets
        DIRECTORY "${directory}"
        PROPERTY BUILDSYSTEM_TARGETS)
    get_property(
        imports
        DIRECTORY "${directory}"
        PROPERTY IMPORTED_TARGETS)
    foreach(target IN LISTS targets imports)
        # Original imported archives are private inputs to our provider. Never mutate their
        # interfaces or replace the provider's own archive links.
        if(target STREQUAL "firefly-openssl"
           OR target MATCHES "^OpenSSL::"
           OR NOT TARGET "${target}")
            continue()
        endif()
        foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
            get_target_property(value "${target}" ${property})
            if(value)
                string(REPLACE "OpenSSL::Crypto" "firefly-openssl" replaced "${value}")
                string(REPLACE "OpenSSL::SSL" "firefly-openssl" replaced "${replaced}")
                string(REPLACE "${OPENSSL_CRYPTO_LIBRARY}" "firefly-openssl" replaced "${replaced}")
                string(REPLACE "${OPENSSL_SSL_LIBRARY}" "firefly-openssl" replaced "${replaced}")
                if(NOT replaced STREQUAL value)
                    set_property(TARGET "${target}" PROPERTY ${property} "${replaced}")
                endif()
            endif()
        endforeach()
    endforeach()
    get_property(
        children
        DIRECTORY "${directory}"
        PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        firefly_use_shared_openssl("${child}")
    endforeach()
endfunction()
firefly_use_shared_openssl("${CMAKE_SOURCE_DIR}")

add_executable(firefly-openssl-probe "${FIREFLY_SOURCE_DIR}/firefly/tests/OpenSSLProviderTest.c")
target_link_libraries(firefly-openssl-probe PRIVATE firefly-openssl)
add_custom_target(
    check-firefly-openssl
    COMMAND "$<TARGET_FILE:firefly-openssl-probe>"
    DEPENDS firefly-openssl-probe
    USES_TERMINAL VERBATIM)
add_dependencies(check-firefly check-firefly-openssl)
