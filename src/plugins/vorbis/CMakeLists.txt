find_package(Vorbis QUIET)

if(NOT Vorbis_FOUND)
    message(STATUS "libvorbisfile not found; skipping Vorbis input plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    vorbis
    DEPENDS Fooyin::Core
            Vorbis::VorbisFile
    SOURCES vorbisdecoder.cpp
            vorbisdecoder.h
            vorbisplugin.cpp
            vorbisplugin.h
)
