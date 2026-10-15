find_package(Jack QUIET)

if(NOT Jack_FOUND)
    return()
endif()

create_fooyin_plugin_internal(
    jack
    NO_PCH
    DEPENDS Fooyin::Core
            Jack::Jack
    SOURCES jackplugin.cpp
            jackoutput.cpp
            jackplugin.h
            jackoutput.h
)
