create_fooyin_plugin_internal(
    notify
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Qt6::DBus
    SOURCES notifyplugin.cpp
            notifyplugin.h
            settings/notifypage.cpp
            settings/notifypage.h
            settings/notifysettings.cpp
            settings/notifysettings.h
)
