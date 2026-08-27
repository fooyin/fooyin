if(NOT UNIX OR APPLE)
    message(STATUS "Audio CD plugin is currently only available on Linux; skipping.")
    return()
endif()

find_package(Cdio QUIET)
if(NOT Cdio_FOUND)
    message(STATUS "libcdio and libcdio-paranoia not found; skipping Audio CD plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    cdda
    DEPENDS Fooyin::Gui
            Fooyin::GuiPrivate
            Cdio::Cdio
    SOURCES accuraterip.cpp
            accuraterip.h
            cddadecoder.cpp
            cddadecoder.h
            cddadrivemanager.cpp
            cddadrivemanager.h
            cddadrivesettings.cpp
            cddadrivesettings.h
            cddadrivesettingsdialog.cpp
            cddadrivesettingsdialog.h
            cddaplugin.cpp
            cddaplugin.h
            cddareader.cpp
            cddareader.h
            cddasectorreader.cpp
            cddasectorreader.h
            cddatoc.cpp
            cddatoc.h
            cddatypes.h
            cddaurl.cpp
            cddaurl.h
            openaudiocddialog.cpp
            openaudiocddialog.h
            ripaudiocddialog.cpp
            ripaudiocddialog.h
            drive/cddrivebackend.cpp
            drive/cddrivebackend.h
            drive/libcdiodrivebackend.cpp
            drive/libcdiodrivebackend.h
)
