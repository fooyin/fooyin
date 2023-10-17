cmake_minimum_required(VERSION 3.14)

find_package(SDL2)

if(NOT SDL2_FOUND)
    return()
endif()

project(
    fooyin_sdl
    VERSION 0.1.0
    DESCRIPTION "SDL2 plugin for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

create_fooyin_plugin(
    fooyin_sdl
    SOURCES sdlplugin.cpp
            sdloutput.cpp
)

target_link_libraries(
    fooyin_sdl
    PRIVATE Fooyin::Core
            ${SDL2_LIBRARIES}
)
