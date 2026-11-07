# This policy has no Qt/VM dependency and can be checked before any desktop starts.
add_library(firefly-runtime-paths STATIC "${CMAKE_CURRENT_LIST_DIR}/RuntimePaths.cpp")
add_executable(firefly-runtime-paths-probe "${CMAKE_CURRENT_LIST_DIR}/RuntimePathsProbe.cpp")
target_link_libraries(firefly-runtime-paths-probe PRIVATE firefly-runtime-paths)
