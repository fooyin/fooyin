create_fooyin_plugin_internal(
    wavebar
    DEPENDS Fooyin::Gui
    SOURCES wavebardatabase.cpp
            wavebardatabase.h
            wavebarplugin.cpp
            wavebarplugin.h
            wavebarwidget.cpp
            wavebarwidget.h
            waveformbuilder.cpp
            waveformbuilder.h
            waveformdata.h
            waveseekbar.cpp
            waveseekbar.h
)
