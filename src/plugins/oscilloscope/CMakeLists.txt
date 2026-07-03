create_fooyin_plugin_internal(
    oscilloscope
    DEPENDS Fooyin::Gui
    SOURCES oscilloscopecolours.h
            oscilloscopeconfigwidget.cpp
            oscilloscopeconfigwidget.h
            oscilloscopeplugin.cpp
            oscilloscopeplugin.h
            oscilloscopewidget.cpp
            oscilloscopewidget.h
)
