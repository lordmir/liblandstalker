cmake_minimum_required(VERSION 3.16)
project(liblandstalker)
set(CMAKE_CXX_STANDARD 20)

include(dependencies.cmake)

# Download dependencies if this is a standalone build
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    InstallDependencies()
endif()

add_subdirectory(landstalker)
