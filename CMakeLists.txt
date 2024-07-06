######################################################################
# @author      : rbrugo
# @file        : CMakeLists
# @created     : Friday Jun 21, 2024 22:02:59 CET
######################################################################

# TO USE CONAN:
#  conan install conanfile.txt --output-folder=build --build=missing --settings=build_type=Release
#  cmake --preset conan-release
#  cmake --build --preset conan-release

cmake_minimum_required(VERSION 3.22.2)

project(ropes VERSION 0.1.1 LANGUAGES CXX)
include(cmake/general.cmake)
include(cmake/functions.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                                Conan                                 #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

find_package(fmt REQUIRED)
find_package(mp-units REQUIRED)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                           Enable warnings                            #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
add_library(project_warnings INTERFACE)
include(cmake/compiler_warnings.cmake)
set_project_warnings(project_warnings)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                             ropes                              #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
add_executable(ropes)
target_sources(ropes PRIVATE src/main.cpp)
target_compile_features(ropes PUBLIC cxx_std_20)
target_compile_options(ropes PRIVATE)
target_link_libraries(ropes PRIVATE fmt::fmt mp-units::mp-units project_warnings)
target_link_options(ropes PRIVATE -fuse-ld=mold)
target_include_directories(ropes PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
enable_sanitizers(ropes)
enable_lto(ropes)
