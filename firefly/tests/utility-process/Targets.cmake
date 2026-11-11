# Actual OS children/native frames; neither fixture opens a desktop window.
add_executable(firefly-utility-port-protocol-probe
    "${CMAKE_CURRENT_LIST_DIR}/PortProtocolProbe.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/utility/ports/Protocol.cpp"
    "${FIREFLY_SOURCE_DIR}/firefly/native/utility/ports/Ledger.cpp")
target_include_directories(firefly-utility-port-protocol-probe PRIVATE
    "${FIREFLY_SOURCE_DIR}/firefly")

add_custom_target(check-firefly-utility-ports
    COMMAND "$<TARGET_FILE:firefly-utility-port-protocol-probe>"
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_utility_ports.py"
            --firefly "$<TARGET_FILE:firefly>"
    DEPENDS firefly firefly-utility-port-protocol-probe
    WORKING_DIRECTORY "${FIREFLY_SOURCE_DIR}"
    VERBATIM)

add_custom_target(
    check-firefly-utility-process
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_utility_process.py"
            --firefly "$<TARGET_FILE:firefly>"
    COMMAND
        "${Python3_EXECUTABLE}"
        "${FIREFLY_SOURCE_DIR}/firefly/tests/utility-process/frame-boundaries.py" --firefly
        "$<TARGET_FILE:firefly>"
    DEPENDS firefly
    WORKING_DIRECTORY "${FIREFLY_SOURCE_DIR}"
    VERBATIM)
