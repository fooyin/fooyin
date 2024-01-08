#
# Fooyin
# Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
#
# Fooyin is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Fooyin is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
#

cmake_minimum_required(VERSION 3.14)

project(
    fooyin_core
    VERSION 0.1.0
    DESCRIPTION "Core library for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(HEADERS
    ${CMAKE_SOURCE_DIR}/include/core/coresettings.h
    ${CMAKE_SOURCE_DIR}/include/core/library/musiclibrary.h
    ${CMAKE_SOURCE_DIR}/include/core/player/playermanager.h
    ${CMAKE_SOURCE_DIR}/include/core/playlist/playlistmanager.h
)

set(SOURCES
    engine/enginehandler.cpp
    engine/audioengine.cpp
    engine/audiooutput.cpp
    engine/ffmpeg/ffmpegclock.cpp
    engine/ffmpeg/ffmpegrenderer.cpp
    engine/ffmpeg/ffmpegworker.cpp
    engine/ffmpeg/ffmpegcodec.cpp
    engine/ffmpeg/ffmpegdecoder.cpp
    engine/ffmpeg/ffmpegengine.cpp
    engine/ffmpeg/ffmpegpacket.cpp
    engine/ffmpeg/ffmpegstream.cpp
    engine/ffmpeg/ffmpegframe.cpp
    engine/ffmpeg/ffmpegutils.cpp
    library/trackfilter.cpp
    library/tracksort.cpp
    player/playercontroller.cpp
    scripting/scriptregistry.cpp
    scripting/scriptparser.cpp
    scripting/scriptscanner.cpp
    scripting/functions/controlfuncs.cpp
    scripting/functions/mathfuncs.cpp
    scripting/functions/stringfuncs.cpp
    scripting/functions/timefuncs.cpp
    track.cpp
)

set(PRIVATE_SOURCES
    application.cpp
    corepaths.cpp
    internalcoresettings.cpp
    database/database.cpp
    database/librarydatabase.cpp
    database/trackdatabase.cpp
    database/databasemodule.cpp
    database/playlistdatabase.cpp
    database/databasequery.cpp
    engine/output/alsaoutput.cpp
    library/libraryscanner.cpp
    library/librarythreadhandler.cpp
    library/libraryinfo.h
    library/librarymanager.cpp
    library/sortingregistry.cpp
    library/trackdatabasemanager.cpp
    library/unifiedmusiclibrary.cpp
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
    SOURCES ${HEADERS} ${SOURCES}
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
