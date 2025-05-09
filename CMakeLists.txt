cmake_minimum_required(VERSION 3.25)
project(task)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_CXX_STANDARD 23)

include(CompilationOptions)

file(GLOB
    LIB_SOURCES
    lib/*.cpp
    lib/*.hpp
)

add_library(lib ${LIB_SOURCES})
target_link_libraries(lib PUBLIC pcap)
append_compilation_options(lib WARNINGS)


find_package(libpcap REQUIRED)
add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} lib)
append_compilation_options(${PROJECT_NAME} OPTIMIZATION)


enable_testing()
file(GLOB
    TESTS_SOURCES
    tests/*.cpp
)

find_package(Catch2 3 REQUIRED)
add_executable(tests ${TESTS_SOURCES})
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tests lib Catch2::Catch2WithMain)

add_test(
    NAME tests
    COMMAND tests -r console
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
