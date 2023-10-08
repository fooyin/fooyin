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
    ${CMAKE_SOURCE_DIR}/include/core/engine/engine.h
    ${CMAKE_SOURCE_DIR}/include/core/coresettings.h
    ${CMAKE_SOURCE_DIR}/include/core/library/libraryinfo.h
    ${CMAKE_SOURCE_DIR}/include/core/library/librarymanager.h
    ${CMAKE_SOURCE_DIR}/include/core/library/musiclibrary.h
    ${CMAKE_SOURCE_DIR}/include/core/library/sortingregistry.h
    ${CMAKE_SOURCE_DIR}/include/core/player/playermanager.h
    ${CMAKE_SOURCE_DIR}/include/core/playlist/playlistmanager.h
    ${CMAKE_SOURCE_DIR}/include/core/plugins/plugininfo.h
)

set(SOURCES
    application.cpp
    corepaths.cpp
    coresettings.cpp
    database/database.cpp
    database/library.cpp
    database/librarydatabase.h
    database/librarydatabase.cpp
    database/module.cpp
    database/playlistdatabase.h
    database/playlistdatabase.cpp
    database/query.cpp
    engine/enginempv.cpp
    engine/enginehandler.cpp
    library/coverprovider.cpp
    library/librarydatabasemanager.cpp
    library/librarymanager.cpp
    library/libraryscanner.cpp
    library/unifiedmusiclibrary.cpp
    library/librarythreadhandler.cpp
    library/sortingregistry.cpp
    library/tracksort.cpp
    playlist/playlist.cpp
    playlist/playlisthandler.cpp
    player/playercontroller.cpp
    plugins/plugininfo.cpp
    plugins/pluginmanager.cpp
    scripting/scriptregistry.cpp
    scripting/scriptparser.cpp
    scripting/scriptscanner.cpp
    scripting/functions/controlfuncs.cpp
    scripting/functions/mathfuncs.cpp
    scripting/functions/stringfuncs.cpp
    scripting/functions/timefuncs.cpp
    tagging/tagreader.cpp
    tagging/tagutils.cpp
    track.cpp
)

create_fooyin_library(
    fooyin_core
    EXPORT_NAME Core
    SOURCES ${HEADERS} ${SOURCES}
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
           Libmpv::Libmpv
           Fooyin::Utils
)
