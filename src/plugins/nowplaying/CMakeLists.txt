if(APPLE)
    message(STATUS "On macOS, enabling NowPlaying plugin")

    set(SOURCES
        nowplayingplugin.mm
        nowplayingplugin.h
    )

    create_fooyin_plugin_internal(
        nowplaying
        DEPENDS Fooyin::Gui
        SOURCES ${SOURCES}
    )

    find_library(MEDIA_PLAYER_FRAMEWORK MediaPlayer REQUIRED)
    target_link_libraries(nowplaying PRIVATE ${MEDIA_PLAYER_FRAMEWORK})
endif()
