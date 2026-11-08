# Real OS-key backend is separate from VM bindings so crypto/keychain tests need no GUI.
add_library(firefly-safe-storage STATIC "${CMAKE_CURRENT_LIST_DIR}/Backend.cpp")
target_link_libraries(firefly-safe-storage PUBLIC OpenSSL::Crypto)
if(APPLE)
    target_link_libraries(firefly-safe-storage PUBLIC "-framework Security"
                                                      "-framework CoreFoundation")
endif()
add_executable(firefly-safe-storage-backend-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/safe-storage/BackendProbe.cpp")
target_include_directories(firefly-safe-storage-backend-probe
                           PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-safe-storage-backend-probe PRIVATE firefly-safe-storage)
add_custom_target(
    check-firefly-safe-storage-backend
    COMMAND "$<TARGET_FILE:firefly-safe-storage-backend-probe>"
    DEPENDS firefly-safe-storage-backend-probe
    VERBATIM)
