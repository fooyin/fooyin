set(SOURCES
    ${CMAKE_SOURCE_DIR}/include/core/coresettings.h
    ${CMAKE_SOURCE_DIR}/include/core/engine/audioengine.h
    ${CMAKE_SOURCE_DIR}/include/core/engine/enginecontroller.h
    ${CMAKE_SOURCE_DIR}/include/core/library/musiclibrary.h

    application.cpp
    corepaths.cpp
    internalcoresettings.cpp
    track.cpp
    translations.cpp

    database/database.cpp
    database/databasequery.cpp
    database/databasemodule.cpp
    database/librarydatabase.cpp
    database/playlistdatabase.cpp
    database/trackdatabase.cpp

    engine/audiobuffer.cpp
    engine/audioconverter.cpp
    engine/audioclock.cpp
    engine/audioformat.cpp
    engine/audioplaybackengine.cpp
    engine/audiorenderer.cpp
    engine/enginehandler.cpp
    engine/ffmpeg/ffmpegcodec.cpp
    engine/ffmpeg/ffmpegdecoder.cpp
    engine/ffmpeg/ffmpegframe.cpp
    engine/ffmpeg/ffmpegpacket.cpp
    engine/ffmpeg/ffmpegstream.cpp
    engine/ffmpeg/ffmpegutils.cpp
    engine/output/alsaoutput.cpp

    library/libraryinfo.h
    library/librarymanager.cpp
    library/libraryscanner.cpp
    library/librarythreadhandler.cpp
    library/librarywatcher.cpp
    library/sortingregistry.cpp
    library/trackdatabasemanager.cpp
    library/trackfilter.cpp
    library/tracksort.cpp
    library/unifiedmusiclibrary.cpp

    player/playercontroller.cpp
    player/playbackqueue.cpp

    plugins/plugininfo.cpp
    plugins/pluginmanager.cpp

    playlist/fyplaylist.cpp
    playlist/playlisthandler.cpp

    scripting/functions/controlfuncs.cpp
    scripting/functions/mathfuncs.cpp
    scripting/functions/stringfuncs.cpp
    scripting/functions/timefuncs.cpp
    scripting/functions/tracklistfuncs.cpp
    scripting/scriptparser.cpp
    scripting/scriptregistry.cpp
    scripting/scriptscanner.cpp

    tagging/tagreader.cpp
    tagging/tagwriter.cpp
)

create_fooyin_library(
    fooyin_core
    ADD_PRIVATE_TARGET
    EXPORT_NAME Core
    SOURCES ${SOURCES}
)

target_link_libraries(
    fooyin_core
    PRIVATE fooyin_version
            fooyin_config
    PUBLIC Qt6::Core
           Qt6::Widgets
           Qt6::Sql
           Qt6::Concurrent
           QCoro6::Core
           Taglib::Taglib
           Fooyin::Utils
           ${FFMPEG_LIBRARIES}
           ${ALSA_LIBRARIES}
)

target_include_directories(
    fooyin_core
    PRIVATE ${FFMPEG_INCLUDE_DIRS}
            ${ALSA_INCLUDE_DIRS}
)
