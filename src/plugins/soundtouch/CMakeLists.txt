find_package(SoundTouch QUIET)

if (NOT SoundTouch_FOUND)
    message(STATUS "SoundTouch not found; skipping soundtouch plugin.")
    return()
endif ()

create_fooyin_plugin_internal(
        soundtouch
        DEPENDS Fooyin::Core
                Fooyin::Gui
                SoundTouch::SoundTouch
        SOURCES soundtouchdsp.cpp
                soundtouchdsp.h
                soundtouchplugin.cpp
                soundtouchplugin.h
                soundtouchsettingswidget.cpp
                soundtouchsettingswidget.h
)
