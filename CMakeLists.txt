cmake_minimum_required(VERSION 3.16)
project(liblandstalker)
set(CMAKE_CXX_STANDARD 20)
include(ExternalProject)

ExternalProject_Add(zlib
    PREFIX zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    CMAKE_CACHE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/zlib
        -DZLIB_BUILD_SHARED:BOOL=OFF
        -DZLIB_BUILD_STATIC:BOOL=ON
        -DZLIB_BUILD_TESTS:BOOL=OFF
        -DZLIB_BUILD_MINIZIP:BOOL=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
)
find_package(ZLIB PATHS ${CMAKE_CURRENT_BINARY_DIR}/zlib)

ExternalProject_Add(libpng
    PREFIX libpng
    GIT_REPOSITORY https://github.com/pnggroup/libpng.git
	INSTALL_DIR "${CMAKE_BINARY_DIR}/libpng"
    CMAKE_CACHE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/libpng
        -DPNG_SHARED:BOOL=OFF
        -DPNG_STATIC:BOOL=ON
        -DPNG_TOOLS:BOOL=OFF
        -DPNG_BUILD_ZLIB:BOOL=OFF
        -DPNG_TESTS:BOOL=OFF
        -DZLIB_ROOT:PATH=${CMAKE_CURRENT_BINARY_DIR}/zlib
)
add_library(png_static STATIC IMPORTED)
set_target_properties(png_static PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/libpng/lib)
set_target_properties(png_static PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/libpng/include)

ExternalProject_Add(yaml_cpp
    PREFIX yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    CMAKE_CACHE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp
        -DYAML_CPP_BUILD_CONTRIB:BOOL=ON
        -DYAML_CPP_BUILD_TOOLS:BOOL=OFF
        -DYAML_BUILD_SHARED_LIBS:BOOL=OFF
        -DYAML_CPP_INSTALL:BOOL=ON
        -DYAML_CPP_FORMAT_SOURCE:BOOL=OFF
        -DYAML_CPP_DISABLE_UNINSTALL:BOOL=ON
        -DYAML_USE_SYSTEM_GTEST:BOOL=OFF
        -DYAML_ENABLE_PIC:BOOL=ON
)
find_package(yaml-cpp PATHS ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp)


add_subdirectory(landstalker)
