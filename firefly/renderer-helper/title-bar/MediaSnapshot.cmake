# Apply the same snapshot layout to every SDK consumer before its headers are selected.
set(title_bar_css_output "${CMAKE_BINARY_DIR}/firefly-adapters/title-bar-css")
set(title_bar_css_prepare "${FIREFLY_SOURCE_DIR}/tooling/prepare_title_bar_css.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${title_bar_css_prepare}" --output
                        "${title_bar_css_output}" RESULT_VARIABLE title_bar_css_result)
if(NOT title_bar_css_result EQUAL 0)
    message(FATAL_ERROR "Review the pinned titlebar CSS snapshot adapter")
endif()
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${title_bar_css_prepare}"
             "${FIREFLY_SOURCE_DIR}/vendor/ladybird/Libraries/LibWeb/CSS/MediaList.cpp"
             "${FIREFLY_SOURCE_DIR}/vendor/ladybird/Libraries/LibWeb/CSS/RustMediaList.h")
include_directories(BEFORE "${title_bar_css_output}/include")
