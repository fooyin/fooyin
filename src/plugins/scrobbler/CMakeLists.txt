find_package(Qt6 COMPONENTS Network QUIET)

if(NOT Qt6Network_FOUND)
    message(STATUS "Qt6::Network not found; skipping scrobbler plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    scrobbler
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Qt6::Network
    SOURCES lastfmservice.cpp
            lastfmservice.h
            scrobbler.cpp
            scrobbler.h
            scrobblerauthsession.cpp
            scrobblerauthsession.h
            scrobblercache.cpp
            scrobblercache.h
            scrobblerpage.cpp
            scrobblerpage.h
            scrobblerplugin.cpp
            scrobblerplugin.h
            scrobblerservice.cpp
            scrobblerservice.h
            scrobblersettings.cpp
            scrobblersettings.h
)
