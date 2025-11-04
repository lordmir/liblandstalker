cmake_minimum_required(VERSION 3.28)
set(MODULE_NAME "landstalker")
project(${MODULE_NAME} VERSION 0.4.0 DESCRIPTION "Landstalker ROM Utility Library")
set(CMAKE_CXX_STANDARD 20)

option(LANDSTALKER_BUILD_SHARED "Build liblandstalker as a shared library" OFF)
option(INSTALL_DEPS "Automatically download and build dependencies" ON)

include(dependencies.cmake)

# Download dependencies if this is a standalone build
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    InstallDependencies()
endif()

if(WIN32)
  set(CMAKE_DEBUG_POSTFIX d)
endif()

if(${LANDSTALKER_BUILD_SHARED})
    if(EMSCRIPTEN)
        message(FATAL_ERROR "LANDSTALKER_BUILD_SHARED can not be used when building for Emscripten")
    endif()

    set( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True )
    add_library(${MODULE_NAME} SHARED)
    set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
    set_target_properties(${MODULE_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
    
    if(WIN32)
        if (MSVC)
            target_compile_definitions(${MODULE_NAME} PRIVATE COMPILEDLL)
        endif()
            target_link_libraries(${MODULE_NAME} PRIVATE ws2_32)
    endif()
    
    set(${MODULE_NAME}_RUNTIME_DEPENDENCIES png_shared yaml-cpp zlib pugixml-shared)
    target_link_libraries(${MODULE_NAME} PRIVATE ZLIB::ZLIB png_shared yaml-cpp::yaml-cpp pugixml::shared)
else()
    add_library(${MODULE_NAME} STATIC)

    if (MSVC)
        target_compile_definitions(${MODULE_NAME} PRIVATE COMPILELIB)
    endif()

    if(EMSCRIPTEN)
        add_compile_options(-sUSE_LIBPNG)
        add_definitions("-sUSE_LIBPNG -fexceptions")
        set(CMAKE_EXE_LINKER_FLAGS "-sASSERTIONS -sUSE_LIBPNG")
        target_link_libraries(${MODULE_NAME} PRIVATE yaml-cpp::yaml-cpp pugixml::static)
    else()
        target_link_libraries(${MODULE_NAME} PRIVATE ZLIB::ZLIB png_static yaml-cpp::yaml-cpp pugixml::static)
    endif()
endif()

if (MSVC)
    target_compile_options(${MODULE_NAME} PRIVATE -D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

set_target_properties(${MODULE_NAME} PROPERTIES LINKER_LANGUAGE CXX)

add_subdirectory("landstalker")

target_include_directories(${MODULE_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(
    TARGETS ${MODULE_NAME} ${${MODULE_NAME}_RUNTIME_DEPENDENCIES}
    CONFIGURATIONS Release Debug
    EXPORT ${MODULE_NAME}Targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
    FILE_SET HEADERS
)
