cmake_minimum_required(VERSION 3.14)

find_package(SDL2)

if(NOT SDL2_FOUND)
    return()
endif()

project(
    fooyin_sdl
    VERSION 0.2.0
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

set_target_properties(
    fooyin_sdl
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
               INSTALL_RPATH ${FOOYIN_PLUGIN_INSTALL_DIR}
)

if(NOT CMAKE_SKIP_INSTALL_RULES)
    install(TARGETS fooyin_sdl DESTINATION ${FOOYIN_PLUGIN_INSTALL_DIR})
endif()
