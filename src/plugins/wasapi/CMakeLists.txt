if(NOT WIN32)
    message(STATUS "WASAPI is only available on Windows; skipping wasapi plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    wasapi
    DEPENDS Fooyin::Core
            ole32
            avrt
    SOURCES wasapioutput.cpp
            wasapioutput.h
            wasapiplugin.cpp
            wasapiplugin.h
)