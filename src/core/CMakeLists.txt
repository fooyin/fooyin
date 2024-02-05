set(SOURCES
    ${CMAKE_SOURCE_DIR}/include/core/coresettings.h
    ${CMAKE_SOURCE_DIR}/include/core/engine/enginecontroller.h
    ${CMAKE_SOURCE_DIR}/include/core/library/musiclibrary.h
    ${CMAKE_SOURCE_DIR}/include/core/player/playermanager.h
    ${CMAKE_SOURCE_DIR}/include/core/playlist/playlistmanager.h
    track.cpp
    engine/audiobuffer.cpp
    engine/audioengine.cpp
    engine/audioformat.cpp
    engine/audiooutput.cpp
    library/trackfilter.cpp
    library/tracksort.cpp
    scripting/scriptregistry.cpp
    scripting/scriptparser.cpp
    scripting/scriptscanner.cpp
    scripting/functions/controlfuncs.cpp
    scripting/functions/mathfuncs.cpp
    scripting/functions/stringfuncs.cpp
    scripting/functions/timefuncs.cpp
    scripting/functions/tracklistfuncs.cpp
)

set(PRIVATE_SOURCES
    application.cpp
    corepaths.cpp
    internalcoresettings.cpp
    translations.cpp
    database/database.cpp
    database/librarydatabase.cpp
    database/trackdatabase.cpp
    database/databasemodule.cpp
    database/playlistdatabase.cpp
    database/databasequery.cpp
    engine/enginehandler.cpp
    engine/ffmpeg/ffmpegclock.cpp
    engine/ffmpeg/ffmpegrenderer.cpp
    engine/ffmpeg/ffmpegcodec.cpp
    engine/ffmpeg/ffmpegdecoder.cpp
    engine/ffmpeg/ffmpegengine.cpp
    engine/ffmpeg/ffmpegpacket.cpp
    engine/ffmpeg/ffmpegstream.cpp
    engine/ffmpeg/ffmpegframe.cpp
    engine/ffmpeg/ffmpegutils.cpp
    engine/output/alsaoutput.cpp
    library/libraryscanner.cpp
    library/librarythreadhandler.cpp
    library/libraryinfo.h
    library/librarymanager.cpp
    library/librarywatcher.cpp
    library/sortingregistry.cpp
    library/trackdatabasemanager.cpp
    library/unifiedmusiclibrary.cpp
    player/playercontroller.cpp
    playlist/fyplaylist.cpp
    playlist/playlisthandler.cpp
    plugins/plugininfo.cpp
    plugins/pluginmanager.cpp
    tagging/tagreader.cpp
    tagging/tagwriter.cpp
)

create_fooyin_library(
    fooyin_core
    EXPORT_NAME Core
    SOURCES ${SOURCES}
    PRIVATE_SOURCES ${PRIVATE_SOURCES}
)

target_link_libraries(
    fooyin_core
    PRIVATE fooyin_version
            fooyin_config
    PUBLIC Qt6::Core
           Qt6::Widgets
           Qt6::Sql
           Qt6::Concurrent
           QCoro::Core
           Taglib::Taglib
           Fooyin::Utils
)

target_link_libraries(
    fooyin_core_private
    PRIVATE fooyin_version
            ${FFMPEG_LIBRARIES}
            ${ALSA_LIBRARIES}
)
