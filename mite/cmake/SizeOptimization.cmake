# Applied after target declaration so the selected size flag follows upstream's Release -O3 options.
# This changes our build graph, never upstream source.
option(ELYTRA_OPTIMIZE_SIZE "Optimize project C++ Release objects for size" OFF)
set(ELYTRA_CPP_SIZE_FLAG
    ""
    CACHE INTERNAL "Selected C++ size optimization flag" FORCE)

function(elytra_size_optimize_directory directory flag)
    get_property(
        targets
        DIRECTORY "${directory}"
        PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type "${target}" TYPE)
        if(type MATCHES
           "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
            target_compile_options("${target}" PRIVATE "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:${flag}>")
        endif()
    endforeach()
    get_property(
        children
        DIRECTORY "${directory}"
        PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        elytra_size_optimize_directory("${child}" "${flag}")
    endforeach()
endfunction()

function(elytra_enable_size_optimization)
    if(NOT ELYTRA_OPTIMIZE_SIZE)
        return()
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR "ELYTRA_OPTIMIZE_SIZE requires a Release build")
    endif()
    if(MSVC)
        set(flag /O1)
    else()
        set(flag -Oz)
    endif()
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("${flag}" ELYTRA_HAS_CPP_SIZE_FLAG)
    if(NOT ELYTRA_HAS_CPP_SIZE_FLAG)
        message(FATAL_ERROR "Compiler does not support requested C++ size flag ${flag}")
    endif()
    elytra_size_optimize_directory("${CMAKE_SOURCE_DIR}" "${flag}")
    set(ELYTRA_CPP_SIZE_FLAG
        "${flag}"
        CACHE INTERNAL "Selected C++ size optimization flag" FORCE)
    message(
        STATUS "Project C++ Release size optimization: ${flag}; Rust/dependency profiles unchanged")
endfunction()
