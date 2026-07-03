create_fooyin_plugin_internal(
    spectrogram
    DEPENDS Fooyin::Gui
    SOURCES spectrogramplugin.cpp
            spectrogramplugin.h
            spectrogramwidget.cpp
            spectrogramwidget.h
            spectrogramconfigwidget.cpp
            spectrogramconfigwidget.h
)
