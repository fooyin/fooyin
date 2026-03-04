create_fooyin_plugin_internal(
    vumeter
    DEPENDS Fooyin::Gui
    SOURCES vumetercolours.h
            vumeterplugin.cpp
            vumeterplugin.h
            vumeterwidget.cpp
            vumeterwidget.h
            vumeterconfigwidget.cpp
            vumeterconfigwidget.h
)
