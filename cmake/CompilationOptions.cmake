# Add compilation options appropriate for the current compiler

# NOTE: https://en.cppreference.com/w/cpp/compiler_support, look for move_only_function
if(APPLE)
    message(FATAL_ERROR, "Not supported because libc++ does not implement std::move_only_function")
endif()

function(append_compilation_options)
    set(options OPTIMIZATION WARNINGS)
    set(Options_NAME ${ARGV0})
    cmake_parse_arguments(Options "${options}" "" "" ${ARGN})

    if(NOT DEFINED Options_NAME)
        message(FATAL_ERROR "NAME must be set")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        if(Options_OPTIMIZATION)
            target_compile_options(${Options_NAME} PRIVATE $<IF:$<CONFIG:Debug>,/Od,/Ox>)
        endif()

        if(Options_WARNINGS)
            # disable C4456: declaration of 'b' hides previous local declaration
            target_compile_options(${Options_NAME} PUBLIC /W4 /wd4456)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(Options_OPTIMIZATION)
            target_compile_options(${Options_NAME} PRIVATE $<IF:$<CONFIG:Debug>,-O0 -fno-omit-frame-pointer,-O2>)
        endif()

        if(Options_WARNINGS)
            target_compile_options(${Options_NAME} PUBLIC -Wall -Wextra -Wpedantic -Werror -Wno-redundant-move)
        endif()

        # NOTE: libc++ does not implement std::move_only_function. Use libstdc++ for clang on Linux.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${Options_NAME} PUBLIC -stdlib=libstdc++)
        endif()
    endif()
endfunction()
