find_package(Mpg123 QUIET)

if(NOT Mpg123_FOUND)
    message(STATUS "mpg123 not found; skipping MPEG audio input plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    mpg123
    DEPENDS Fooyin::Core
            Mpg123::Mpg123
    SOURCES mpegdecoder.cpp
            mpegdecoder.h
            mpg123plugin.cpp
            mpg123plugin.h
)
