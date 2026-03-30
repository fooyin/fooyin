find_package(Qt6 COMPONENTS DBus QUIET)

if(NOT Qt6DBus_FOUND)
    message(STATUS "Qt6::DBus not found; skipping notify plugin.")
    return()
endif()

set(SOURCES
    notifyplugin.cpp
    notifyplugin.h
    settings/notifypage.cpp
    settings/notifypage.h
    settings/notifysettings.cpp
    settings/notifysettings.h
)

qt_add_dbus_interface(SOURCES org.freedesktop.Notifications.xml notificationsinterface)

create_fooyin_plugin_internal(
    notify
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Qt6::DBus
    SOURCES ${SOURCES}
)
