message(STATUS "On macOS, enabling NowPlaying plugin")

set(SOURCES
    #[[gersemi: sort]] #
    nowplayingplugin.h
    nowplayingplugin.mm
)

create_fooyin_plugin_internal(nowplaying NO_PCH DEPENDS Fooyin::Gui SOURCES ${SOURCES})

find_library(MEDIA_PLAYER_FRAMEWORK MediaPlayer REQUIRED)
target_link_libraries(nowplaying PRIVATE ${MEDIA_PLAYER_FRAMEWORK})
