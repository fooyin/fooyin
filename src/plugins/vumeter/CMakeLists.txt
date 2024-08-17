create_fooyin_plugin_internal(
    vumeter
    DEPENDS Fooyin::Gui
    SOURCES vumetercolours.h
            vumeterplugin.cpp
            vumeterplugin.h
            vumetersettings.cpp
            vumetersettings.h
            vumetersettingspage.cpp
            vumetersettingspage.h
            vumeterwidget.cpp
            vumeterwidget.h
)
