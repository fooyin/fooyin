cmake_minimum_required(VERSION 3.14)

project(
  fooyin_core
  VERSION 0.1.0
  DESCRIPTION "Core library for fooyin"
  HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
  LANGUAGES CXX)

set(SOURCES
    coreplugin.h
    coreplugincontext.h
    corepaths.h
    corepaths.cpp
    coresettings.h
    coresettings.cpp
    constants.h
    typedefs.h
    database/database.h
    database/database.cpp
    database/library.h
    database/library.cpp
    database/librarydatabase.h
    database/librarydatabase.cpp
    database/module.h
    database/module.cpp
    database/playlistdatabase.h
    database/playlistdatabase.cpp
    database/query.h
    database/query.cpp
    engine/engine.h
    engine/enginempv.h
    engine/enginempv.cpp
    engine/enginehandler.h
    engine/enginehandler.cpp
    library/coverprovider.h
    library/coverprovider.cpp
    library/librarydatabasemanager.h
    library/librarydatabasemanager.cpp
    library/libraryinfo.h
    library/libraryinteractor.h
    library/librarymanager.h
    library/librarymanager.cpp
    library/libraryscanner.h
    library/libraryscanner.cpp
    library/musiclibrary.h
    library/unifiedmusiclibrary.h
    library/unifiedmusiclibrary.cpp
    library/librarythreadhandler.h
    library/librarythreadhandler.cpp
    library/tracksorter.h
    library/tracksorter.cpp
    models/container.h
    models/container.cpp
    models/album.h
    models/album.cpp
    models/artist.h
    models/artist.cpp
    models/track.h
    models/track.cpp
    models/trackfwd.h
    playlist/playlist.h
    playlist/playlist.cpp
    playlist/playlisthandler.h
    playlist/playlisthandler.cpp
    player/playercontroller.h
    player/playercontroller.cpp
    player/playermanager.h
    plugins/plugininfo.h
    plugins/plugininfo.cpp
    plugins/pluginmanager.h
    plugins/pluginmanager.cpp
    scripting/scriptregistry.cpp
    scripting/scriptparser.cpp
    scripting/scriptscanner.cpp
    scripting/functions/controlfuncs.cpp
    scripting/functions/mathfuncs.cpp
    scripting/functions/stringfuncs.cpp
    scripting/functions/timefuncs.cpp
    tagging/tagreader.h
    tagging/tagreader.cpp
    tagging/tagutils.h
    tagging/tagutils.cpp)

add_library(fooyin_core ${FOOYIN_LIBRARY_TYPE} ${SOURCES})
add_library(fooyin::core ALIAS fooyin_core)

target_include_directories(
  fooyin_core
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_BINARY_DIR}/src"
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>")

set_target_properties(
  fooyin_core
  PROPERTIES VERSION "${PROJECT_VERSION}"
             EXPORT_NAME fooyin_core
             OUTPUT_NAME fooyin_core
             LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_LIBRARY_OUTPUT_DIR}
             INSTALL_RPATH ${FOOYIN_LIBRARY_INSTALL_PATH})

target_link_libraries(
  fooyin_core
  PRIVATE Qt6::Core
          Qt6::Widgets
          Qt6::Sql
          Taglib::Taglib
          Libmpv::Libmpv
          fooyin::version
          fooyin::utils)

target_compile_features(fooyin_core PUBLIC cxx_std_17)
target_compile_options(fooyin_core PRIVATE -Werror -Wall -Wextra -Wpedantic)

install(TARGETS fooyin_core LIBRARY DESTINATION ${FOOYIN_LIBRARY_DIR})
