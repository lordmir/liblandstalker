cmake_minimum_required(VERSION 3.16)
set(MODULE_NAME "landstalker")
project(${MODULE_NAME})
set(CMAKE_CXX_STANDARD 20)

add_library(${MODULE_NAME} OBJECT)
set_property(TARGET ${MODULE_NAME} PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(${MODULE_NAME}_static STATIC $<TARGET_OBJECTS:${MODULE_NAME}>)
set_target_properties(${MODULE_NAME}_static PROPERTIES OUTPUT_NAME ${MODULE_NAME})

if(NOT EMSCRIPTEN)
    add_library(${MODULE_NAME}_shared SHARED $<TARGET_OBJECTS:${MODULE_NAME}>)
    set_target_properties(${MODULE_NAME}_shared PROPERTIES OUTPUT_NAME ${MODULE_NAME})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${MODULE_NAME} PRIVATE -D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

add_subdirectory("src")
target_include_directories(${MODULE_NAME} PUBLIC "include")

if(EMSCRIPTEN)
    add_compile_options(-sUSE_LIBPNG)
    add_definitions("-sUSE_LIBPNG -fexceptions")
    set(CMAKE_EXE_LINKER_FLAGS "-sASSERTIONS -sUSE_LIBPNG")
else()
    if(WIN32)
        target_link_libraries(${MODULE_NAME} PRIVATE ws2_32)
        target_link_libraries(${MODULE_NAME}_shared PRIVATE ws2_32)
    endif()
    target_link_libraries(${MODULE_NAME} PRIVATE png_static)
    target_link_libraries(${MODULE_NAME}_shared PRIVATE png_static yaml-cpp::yaml-cpp)
endif()

target_link_libraries(${MODULE_NAME} PRIVATE yaml-cpp::yaml-cpp)
