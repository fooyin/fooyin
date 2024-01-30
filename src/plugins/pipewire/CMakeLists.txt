cmake_minimum_required(VERSION 3.14)

find_package(PipeWire)

if(NOT PIPEWIRE_FOUND)
    return()
endif()

project(
    fooyin_pipewire
    VERSION 0.3.0
    DESCRIPTION "PipeWire plugin for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

create_fooyin_plugin(
    fooyin_pipewire
    SOURCES pipewireplugin.cpp
            pipewireoutput.cpp
)

target_link_libraries(
    fooyin_pipewire
    PRIVATE Fooyin::Core
            PipeWire::PipeWire
)
