get_property(firefly_control_selected GLOBAL PROPERTY FIREFLY_BYTE_GENERATOR_SELECTED)
if(NOT firefly_control_selected)
    message(
        FATAL_ERROR
            "Owned byte bootstrap generator was not selected; review upstream CMake include order")
endif()
target_sources(LibIPC PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Bootstrap.cpp")
target_include_directories(LibIPC PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
# Stage the owned declaration under the existing binary include root. Do not add a transitive
# include path to every renderer target just for this native hook.
configure_file("${CMAKE_CURRENT_LIST_DIR}/Bootstrap.h"
               "${CMAKE_BINARY_DIR}/Libraries/LibIPC/FireflyBootstrap.h" COPYONLY)
if(ENABLE_INSTALL_HEADERS)
    install(
        FILES "${CMAKE_CURRENT_LIST_DIR}/Bootstrap.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/LibIPC"
        RENAME FireflyBootstrap.h)
endif()
add_custom_target(
    check-firefly-byte-codegen
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_byte_codegen.py" --output
            "${CMAKE_BINARY_DIR}/firefly-adapters/code_generators.cmake"
    VERBATIM)
add_dependencies(LibIPC check-firefly-byte-codegen)
