create_fooyin_plugin_internal(
    spectrum
    DEPENDS Fooyin::Gui
    SOURCES spectrumaxisrenderer.cpp
            spectrumaxisrenderer.h
            spectrumcolours.h
            spectrumconfigwidget.cpp
            spectrumconfigwidget.h
            spectrumplugin.cpp
            spectrumplugin.h
            spectrumsettings.h
            spectrumview.cpp
            spectrumview.h
            spectrumwidget.cpp
            spectrumwidget.h
)
