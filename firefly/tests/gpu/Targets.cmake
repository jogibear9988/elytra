# Native CPU selection and actual wait-status semantics require no graphics or VM.
add_executable(firefly-gpu-state-probe "${CMAKE_CURRENT_LIST_DIR}/StateProbe.cpp")
target_include_directories(firefly-gpu-state-probe PRIVATE "${FIREFLY_SOURCE_DIR}/firefly")
target_link_libraries(firefly-gpu-state-probe PRIVATE firefly-gpu-policy)
add_custom_target(
    check-firefly-gpu-state
    COMMAND "$<TARGET_FILE:firefly-gpu-state-probe>"
    DEPENDS firefly-gpu-state-probe
    VERBATIM)
if(TARGET firefly-view-offscreen)
    add_custom_target(
        check-firefly-gpu-process
        COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/tests/test_gpu_process.py"
                --firefly "$<TARGET_FILE:firefly-view-offscreen>"
        DEPENDS firefly-view-offscreen
        VERBATIM)
    add_custom_target(
        check-firefly-gpu-public
        COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
                "$<TARGET_FILE:firefly-view-offscreen>" "${CMAKE_CURRENT_LIST_DIR}/public.mjs"
        COMMAND
            "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
            "$<TARGET_FILE:firefly-view-offscreen>" "${CMAKE_CURRENT_LIST_DIR}/public.mjs" --unknown
        DEPENDS firefly-view-offscreen
        VERBATIM)
endif()
