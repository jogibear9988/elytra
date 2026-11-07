# Upstream HSTS reimports its common codegen functions; retain them and reapply the owned selector.
include("${LADYBIRD_SOURCE_DIR}/Meta/CMake/hsts_preload.cmake")
include("${FIREFLY_SOURCE_DIR}/firefly/cmake/ByteCodegen.cmake")
