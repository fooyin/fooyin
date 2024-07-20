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
