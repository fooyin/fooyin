find_package(
    Qt6
    COMPONENTS DBus
    QUIET
)

if(NOT Qt6DBus_FOUND)
    message(STATUS "Qt6::DBus not found; skipping notify plugin.")
    return()
endif()

set(SOURCES
    notificationbackend.cpp
    notificationbackend.h
    notifyplugin.cpp
    notifyplugin.h
    backends/freedesktopnotificationbackend.cpp
    backends/freedesktopnotificationbackend.h
    backends/portalnotificationbackend.cpp
    backends/portalnotificationbackend.h
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
