cmake_minimum_required(VERSION 2.6)
project (pregomr)

enable_testing()

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

include(${CMAKE_SOURCE_DIR}/cmake/internal.cmake)

include_directories("${PROJECT_SOURCE_DIR}")
add_subdirectory(base)
add_subdirectory(strings)
add_subdirectory(file)
add_subdirectory(util)
add_subdirectory(experimental)

