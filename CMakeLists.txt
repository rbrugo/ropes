######################################################################
# @author      : rbrugo
# @file        : CMakeLists
# @created     : Friday Jun 21, 2024 22:02:59 CET
######################################################################

# TO USE CONAN:
#  conan install . --output-folder=build --build=missing --settings=build_type=Release
#  cmake --preset conan-release
#  cmake --build --preset conan-release

# TO USE CONAN (using system-installed versions of OpenGL, SDL2 and SDL2_ttf):
# conan install . \
#     -o check_opengl_compatibility=no \
#     -o opengl_source=system \
#     -o sdl_source=system \
#     --output-folder=build \
#     --build=missing \
#     --settings=build_type=Release
#
#  cmake --preset conan-release
#  cmake --build --preset conan-release

cmake_minimum_required(VERSION 3.22.2)

project(ropes VERSION 0.1.1 LANGUAGES CXX)
include(cmake/general.cmake)
include(cmake/functions.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                            Dependencies                                #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})

find_package(fmt REQUIRED)
find_package(mp-units REQUIRED)
find_package(scope-lite REQUIRED)
find_package(structopt REQUIRED)
find_package(imgui REQUIRED)
find_package(implot REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(OpenGL REQUIRED)

if(SDL2_ttf_FOUND AND NOT TARGET SDL2_ttf::SDL2_ttf)
    add_library(SDL2_ttf::SDL2_ttf INTERFACE IMPORTED)
    target_include_directories(SDL2_ttf::SDL2_ttf INTERFACE ${SDL2_ttf_INCLUDE_DIRS})
    target_link_libraries(SDL2_ttf::SDL2_ttf INTERFACE ${SDL2_ttf_LIBRARIES})
    target_compile_definitions(SDL2_ttf::SDL2_ttf INTERFACE ${SDL2_ttf_DEFINITIONS})
endif()

if(OpenGL_FOUND AND NOT TARGET OpenGL::OpenGL)
    add_library(OpenGL::OpenGL INTERFACE IMPORTED)
    target_include_directories(OpenGL::OpenGL INTERFACE ${OPENGL_INCLUDE_DIR})
    target_link_libraries(OpenGL::OpenGL INTERFACE ${OPENGL_LIBRARIES})
endif()


# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                             Enable warnings                            #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
add_library(project_warnings INTERFACE)
include(cmake/compiler_warnings.cmake)
set_project_warnings(project_warnings)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                                  SDL2                                  #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
add_library(SDL_collection)
target_sources(SDL_collection
    PRIVATE
        src/graphics.cpp
        src/imgui_impl_sdl2.cpp
        src/imgui_impl_opengl3.cpp
)
target_compile_features(SDL_collection PUBLIC cxx_std_23)
target_compile_definitions(SDL_collection PUBLIC MP_UNITS_API_STD_FORMAT=0)
target_link_libraries(SDL_collection
    PUBLIC
        fmt::fmt
        nonstd::scope-lite
        mp-units::mp-units
        imgui::imgui implot::implot
        SDL2::SDL2
        SDL2_ttf::SDL2_ttf
        OpenGL::OpenGL
)
target_include_directories(SDL_collection PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_options(SDL_collection PRIVATE -fuse-ld=mold)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                               Expression                               #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
add_library(expression)
target_sources(expression PRIVATE src/expression.cpp)
target_link_libraries(expression PUBLIC fmt::fmt)
target_include_directories(expression PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_options(expression PRIVATE -fuse-ld=mold)

add_executable(expression_test)
target_sources(expression_test PRIVATE test/expression.cpp)
target_link_libraries(expression_test PUBLIC expression)
target_link_options(expression_test PRIVATE -fuse-ld=mold)
enable_sanitizers(expression_test)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                                 ropes                                  #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
add_executable(ropes)
target_sources(ropes PRIVATE src/main.cpp src/simulation.cpp)
target_compile_features(ropes PUBLIC cxx_std_23)
target_compile_options(ropes PRIVATE)
target_compile_definitions(ropes PUBLIC MP_UNITS_API_STD_FORMAT=0)
target_link_libraries(ropes
    PRIVATE
        fmt::fmt
        mp-units::mp-units
        structopt::structopt
        imgui::imgui
        SDL_collection
        expression
        project_warnings
)
target_link_options(ropes PRIVATE -fuse-ld=mold)
target_include_directories(ropes PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
enable_sanitizers(ropes)
enable_lto(ropes)
enable_profiling(ropes)
