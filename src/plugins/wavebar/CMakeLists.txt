create_fooyin_plugin_internal(
    wavebar
    DEPENDS Fooyin::Gui
    SOURCES settings/wavebarsettings.cpp
            settings/wavebarsettings.h
            wavebarcolours.h
            wavebarconstants.cpp
            wavebarconstants.h
            wavebardatabase.cpp
            wavebardatabase.h
            wavebarplugin.cpp
            wavebarplugin.h
            wavebarwidget.cpp
            wavebarwidget.h
            wavebarconfigwidget.cpp
            wavebarconfigwidget.h
            waveformbuilder.cpp
            waveformbuilder.h
            waveformdata.h
            waveformgenerator.cpp
            waveformgenerator.h
            waveformrescaler.cpp
            waveformrescaler.h
            waveseekbar.cpp
            waveseekbar.h
)
