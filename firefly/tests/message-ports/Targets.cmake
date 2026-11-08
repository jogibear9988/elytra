# Genuine native codec and GC acceptance without QApplication or any desktop window.
if(TARGET firefly-mite)
    add_executable(firefly-message-port-lifetime-probe
                   "${FIREFLY_SOURCE_DIR}/firefly/tests/message-ports/LifetimeProbe.cpp")
    target_include_directories(firefly-message-port-lifetime-probe
                               PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
    target_link_libraries(firefly-message-port-lifetime-probe PRIVATE firefly-mite
                                                                      firefly-isolated-world)
    add_dependencies(firefly-message-port-lifetime-probe check-firefly-upstream)
    ladybird_check_skia_confinement(firefly-message-port-lifetime-probe)
    if(WIN32)
        ladybird_windows_bin(firefly-message-port-lifetime-probe CONSOLE)
    endif()
    if(TARGET firefly)
        add_custom_target(
            check-firefly-message-ports
            COMMAND
                "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_message_ports.py"
                --firefly "$<TARGET_FILE:firefly>"
            DEPENDS firefly
            WORKING_DIRECTORY "${FIREFLY_SOURCE_DIR}"
            VERBATIM)
    endif()
    add_custom_target(
        check-firefly-message-port-lifetime
        COMMAND "$<TARGET_FILE:firefly-message-port-lifetime-probe>"
                "${FIREFLY_SOURCE_DIR}/firefly/tests/message-ports/lifetime.mjs"
        DEPENDS firefly-message-port-lifetime-probe
        WORKING_DIRECTORY "${FIREFLY_SOURCE_DIR}"
        VERBATIM)
endif()
