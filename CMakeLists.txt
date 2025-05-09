cmake_minimum_required(VERSION 3.25)
project(task)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_CXX_STANDARD 23)

include(CompilationOptions)

add_executable(${PROJECT_NAME} main.cpp)
append_compilation_options(${PROJECT_NAME} WARNINGS OPTIMIZATION)
