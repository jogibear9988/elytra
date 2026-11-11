# Included after the raw byte-channel target exists; no VM/renderer dependency.
include_guard(GLOBAL)
target_sources(firefly-byte-channel PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Fragment.cpp")
add_executable(firefly-application-fragment-probe
               "${FIREFLY_SOURCE_DIR}/firefly/tests/ipc/FragmentProbe.cpp")
target_link_libraries(firefly-application-fragment-probe PRIVATE firefly-byte-channel LibMain)
ladybird_check_skia_confinement(firefly-application-fragment-probe)
if(WIN32)
    ladybird_windows_bin(firefly-application-fragment-probe CONSOLE)
endif()
add_custom_target(
    check-firefly-application-fragments
    COMMAND "$<TARGET_FILE:firefly-application-fragment-probe>"
    DEPENDS firefly-application-fragment-probe
    VERBATIM)

add_dependencies(check-firefly-byte-channel check-firefly-application-fragments)
