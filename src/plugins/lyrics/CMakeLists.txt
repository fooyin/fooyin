create_fooyin_plugin_internal(
    lyrics
    DEPENDS Fooyin::Gui
    SOURCES lyrics.h
            lyricsarea.cpp
            lyricsarea.h
            lyricsconstants.h
            lyricsparser.cpp
            lyricsparser.h
            lyricsplugin.cpp
            lyricsplugin.h
            lyricswidget.cpp
            lyricswidget.h
            settings/lyricssettings.cpp
            settings/lyricssettings.h
            settings/lyricsgeneralpage.cpp
            settings/lyricsgeneralpage.h
            settings/lyricsguipage.cpp
            settings/lyricsguipage.h
)
