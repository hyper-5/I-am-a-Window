cmake_minimum_required(VERSION 3.25)

project(i_am_a_window)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
endif()
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.hpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

setup_geode_mod(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} d3d11 dxgi d2d1 dcomp)