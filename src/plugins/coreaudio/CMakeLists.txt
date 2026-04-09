message(STATUS "On macOS, enabling CoreAudio output plugin")

find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)

set(SOURCES
    coreaudioplugin.cpp
    coreaudioplugin.h
    coreaudiooutput.cpp
    coreaudiooutput.h
)

create_fooyin_plugin_internal(
    coreaudio
    DEPENDS Fooyin::Core
            ${AUDIOTOOLBOX_FRAMEWORK}
            ${COREAUDIO_FRAMEWORK}
            ${COREFOUNDATION_FRAMEWORK}
    SOURCES ${SOURCES}
)
