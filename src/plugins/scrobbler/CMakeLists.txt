create_fooyin_plugin_internal(
    scrobbler
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Qt6::Network
    SOURCES lastfmservice.cpp
            lastfmservice.h
            listenbrainzservice.cpp
            listenbrainzservice.h
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
