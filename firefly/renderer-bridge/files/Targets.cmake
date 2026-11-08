# Track native File creation without modifying the selected upstream checkout or its ABI.
set(file_origin_input "${CMAKE_BINARY_DIR}/firefly-adapters/HTMLInputElement.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_origin_hook.py"
            --output "${file_origin_input}" RESULT_VARIABLE file_origin_check)
if(NOT file_origin_check EQUAL 0)
    message(FATAL_ERROR "Review the native file-origin adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_origin_hook.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/HTMLInputElement.cpp")
get_target_property(file_origin_sources LibWeb SOURCES)
set(file_origin_replaced 0)
set(file_origin_adapted)
foreach(source IN LISTS file_origin_sources)
    if(source STREQUAL "HTML/HTMLInputElement.cpp"
       OR source STREQUAL "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/HTML/HTMLInputElement.cpp")
        list(APPEND file_origin_adapted "${file_origin_input}")
        math(EXPR file_origin_replaced "${file_origin_replaced}+1")
    else()
        list(APPEND file_origin_adapted "${source}")
    endif()
endforeach()
if(NOT file_origin_replaced EQUAL 1)
    message(FATAL_ERROR "Review LibWeb input element source layout")
endif()
set_property(TARGET LibWeb PROPERTY SOURCES "${file_origin_adapted}")
target_sources(LibWeb PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Origins.cpp"
                              "${CMAKE_CURRENT_LIST_DIR}/Requests.cpp")
set_property(
    SOURCE "${file_origin_input}" "${CMAKE_CURRENT_LIST_DIR}/Origins.cpp"
           "${CMAKE_CURRENT_LIST_DIR}/Requests.cpp" TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge"
             "${FIREFLY_SOURCE_DIR}/firefly")
add_custom_target(
    check-firefly-file-origin-hook
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_origin_hook.py"
            --output "${file_origin_input}"
    VERBATIM)
add_dependencies(LibWeb check-firefly-file-origin-hook)

set(file_request_page "${CMAKE_BINARY_DIR}/firefly-adapters/FileRequestPage.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_request_hook.py"
            --output "${file_request_page}" RESULT_VARIABLE file_request_check)
if(NOT file_request_check EQUAL 0)
    message(FATAL_ERROR "Review the native file request ownership adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_request_hook.py"
             "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/Page/Page.cpp")
get_target_property(file_request_sources LibWeb SOURCES)
set(file_request_replaced 0)
set(file_request_adapted)
foreach(source IN LISTS file_request_sources)
    if(source STREQUAL "Page/Page.cpp" OR source STREQUAL
                                          "${LADYBIRD_SOURCE_DIR}/Libraries/LibWeb/Page/Page.cpp")
        list(APPEND file_request_adapted "${file_request_page}")
        math(EXPR file_request_replaced "${file_request_replaced}+1")
    else()
        list(APPEND file_request_adapted "${source}")
    endif()
endforeach()
if(NOT file_request_replaced EQUAL 1)
    message(FATAL_ERROR "Review LibWeb Page source layout")
endif()
set_property(TARGET LibWeb PROPERTY SOURCES "${file_request_adapted}")
set_property(
    SOURCE "${file_request_page}" TARGET_DIRECTORY LibWeb
    APPEND
    PROPERTY INCLUDE_DIRECTORIES "${FIREFLY_SOURCE_DIR}/firefly/renderer-bridge")
add_custom_target(
    check-firefly-file-request-hook
    COMMAND "${Python3_EXECUTABLE}" "${FIREFLY_SOURCE_DIR}/tooling/prepare_file_request_hook.py"
            --output "${file_request_page}"
    VERBATIM)
add_dependencies(LibWeb check-firefly-file-request-hook)
