find_package(SDL2 QUIET)

if(NOT SDL2_FOUND)
    return()
endif()

create_fooyin_plugin_internal(
    sdl
    DEPENDS Fooyin::Core
            ${SDL2_LIBRARIES}
    SOURCES sdlplugin.cpp
            sdloutput.cpp
)
