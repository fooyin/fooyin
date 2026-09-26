find_package(OpusFile QUIET)

if(NOT OpusFile_FOUND)
    message(STATUS "opusfile not found; skipping native Opus input plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    opus
    DEPENDS Fooyin::Core
            OpusFile::OpusFile
    SOURCES opusdecoder.cpp
            opusdecoder.h
            opusplugin.cpp
            opusplugin.h
)
