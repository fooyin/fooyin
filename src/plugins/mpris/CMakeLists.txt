find_package(Qt6 COMPONENTS DBus QUIET)

if(NOT Qt6DBus_FOUND)
    message(STATUS "Qt6::DBus not found; skipping mpris plugin.")
    return()
endif()

set(SOURCES
    mprisplugin.cpp
    mprisplugin.h
)

qt_add_dbus_adaptor(SOURCES org.mpris.MediaPlayer2.xml mprisplugin.h Fooyin::Mpris::MprisPlugin mprisroot MprisRoot)
qt_add_dbus_adaptor(SOURCES org.mpris.MediaPlayer2.Player.xml mprisplugin.h Fooyin::Mpris::MprisPlugin mprisplayer MprisPlayer)

create_fooyin_plugin_internal(
    mpris
    DEPENDS Fooyin::Gui
            Qt6::DBus
    SOURCES ${SOURCES}
)

