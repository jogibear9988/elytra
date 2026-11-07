# Use upstream target definitions, then select our narrow protocol-generator adapter.
include("${LADYBIRD_SOURCE_DIR}/Meta/CMake/targets.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/cmake/ByteCodegen.cmake")
