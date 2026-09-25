find_package(FLAC QUIET)

if(NOT FLAC_FOUND)
    message(STATUS "FLAC not found; skipping native FLAC input plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    flac
    DEPENDS Fooyin::Core
            FLAC::FLAC
    SOURCES flacdecoder.cpp
            flacdecoder.h
            flacencoder.cpp
            flacencoder.h
            flacplugin.cpp
            flacplugin.h
)
