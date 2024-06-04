find_package(PulseAudio QUIET)

if(NOT PulseAudio_FOUND)
    message(STATUS "PulseAudio not found; skipping pulseaudio plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    pulseaudio
    DEPENDS Fooyin::Core
            ${PULSEAUDIO_LIBRARY}
    SOURCES pulseaudiooutput.cpp
            pulseaudiooutput.h
            pulseaudioplugin.cpp
            pulseaudioplugin.h
)