create_fooyin_plugin_internal(
    wavebar
    DEPENDS Fooyin::Gui
    SOURCES wavebarcolours.h
            wavebarconstants.h
            wavebardatabase.cpp
            wavebardatabase.h
            wavebarplugin.cpp
            wavebarplugin.h
            wavebarwidget.cpp
            wavebarwidget.h
            waveformbuilder.cpp
            waveformbuilder.h
            waveformdata.h
            waveformgenerator.cpp
            waveformgenerator.h
            waveformrescaler.cpp
            waveformrescaler.h
            waveseekbar.cpp
            waveseekbar.h
            settings/wavebarguisettingspage.cpp
            settings/wavebarguisettingspage.h
            settings/wavebarsettings.cpp
            settings/wavebarsettings.h
            settings/wavebarsettingspage.cpp
            settings/wavebarsettingspage.h
)
