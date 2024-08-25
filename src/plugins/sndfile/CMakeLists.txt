find_package(SndFile QUIET)

if(NOT SndFile_FOUND)
    message(STATUS "SndFile not found; skipping sndfile plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    sndfile
    DEPENDS Fooyin::Core
            SndFile::sndfile
    SOURCES sndfileinput.cpp
            sndfileinput.h
            sndfileplugin.cpp
            sndfileplugin.h
)

fooyin_append_mimes(
    audio/aiff
    audio/basic
    audio/x-sph
    audio/x-voc
    audio/wav
    audio/x-wavex
    audio/x-w64
    audio/x-wve
)
