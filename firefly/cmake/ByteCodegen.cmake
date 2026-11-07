# Redefine only the pinned codegen function through a verified generated copy. Upstream source
# remains untouched; all ordinary endpoints still invoke its original generator.
set(firefly_byte_codegen "${CMAKE_BINARY_DIR}/firefly-adapters/code_generators.cmake")
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_byte_codegen.py"
             "${LADYBIRD_SOURCE_DIR}/Meta/CMake/code_generators.cmake")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_byte_codegen.py" --output
            "${firefly_byte_codegen}" RESULT_VARIABLE firefly_codegen_check)
if(NOT firefly_codegen_check EQUAL 0)
    message(FATAL_ERROR "Review native byte bootstrap generator integration")
endif()
include("${firefly_byte_codegen}")
