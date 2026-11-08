# Actual OS children/native frames; neither fixture opens a desktop window.
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
