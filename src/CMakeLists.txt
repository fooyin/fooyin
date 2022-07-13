
configure_file(version.h.in "utils/version.h" @ONLY)

set(HEADERS
    app/application.h
    app/threadmanager.h

    core/engine/engine.h
    core/engine/enginempv.h
    core/engine/enginehandler.h
    core/library/libraryinfo.h
    core/library/librarymanager.h
    core/library/librarydatabasemanager.h
    core/library/musiclibrary.h
    core/player/playercontroller.h
    core/playlist/libraryplaylistinterface.h
    core/playlist/libraryplaylistmanager.h
    core/playlist/playlist.h
    core/playlist/playlisthandler.h
    core/player/playermanager.h
    core/scanner/libraryscanner.h
    core/tagging/tags.h
    core/tagging/tagutils.h

    database/database.h
    database/library.h
    database/librarydatabase.h
    database/module.h
    database/playlistdatabase.h
    database/query.h

    gui/mainwindow.h
    gui/editablelayout.h
    gui/quicksetupdialog.h

    gui/controls/controlwidget.h
    gui/controls/playercontrol.h
    gui/controls/playlistcontrol.h
    gui/controls/progresswidget.h
    gui/controls/volumecontrol.h

    gui/filter/filterdelegate.h
    gui/filter/filteritem.h
    gui/filter/filtermanager.h
    gui/filter/filtermodel.h
    gui/filter/filterview.h
    gui/filter/filterwidget.h

    gui/info/itemdelegate.h
    gui/info/infomodel.h
    gui/info/infoitem.h
    gui/info/infowidget.h
    gui/info/infoview.h

    gui/library/coverwidget.h
    gui/library/searchwidget.h
    gui/library/statuswidget.h

    gui/playlist/playlistview.h
    gui/playlist/playlistmodel.h
    gui/playlist/playlistitem.h
    gui/playlist/playlistdelegate.h
    gui/playlist/playlistwidget.h

    gui/settings/settingsdialog.h
    gui/settings/settingspages.h

    gui/widgets/clickablelabel.h
    gui/widgets/dummy.h
    gui/widgets/hovermenu.h
    gui/widgets/overlay.h
    gui/widgets/menuheader.h
    gui/widgets/nolibraryoverlay.h
    gui/widgets/slider.h
    gui/widgets/spacer.h
    gui/widgets/splitter.h
    gui/widgets/splitterhandle.h
    gui/widgets/splitterwidget.h
    gui/widgets/widget.h

    models/container.h
    models/libraryitem.h
    models/album.h
    models/artist.h
    models/disc.h
    models/track.h

    utils/coverprovider.h
    utils/enumhelper.h
    utils/filterfwd.h
    utils/helpers.h
    utils/sortorder.h
    utils/sorting.h
    utils/paths.h
    utils/trackfwd.h
    utils/typedefs.h
    utils/utils.h
    utils/settings.h
    utils/widgetfactory.h
    utils/widgetprovider.h
    utils/worker.h
    utils/version.h
)

set(SOURCES
    main.cpp

    app/application.cpp
    app/threadmanager.cpp

    core/engine/enginempv.cpp
    core/engine/enginehandler.cpp
    core/library/libraryinfo.cpp
    core/library/librarymanager.cpp
    core/library/musiclibrary.cpp
    core/library/librarydatabasemanager.cpp
    core/player/playercontroller.cpp
    core/playlist/libraryplaylistmanager.cpp
    core/playlist/playlist.cpp
    core/playlist/playlisthandler.cpp
    core/playlist/playlistmanager.h
    core/scanner/libraryscanner.cpp
    core/tagging/tags.cpp
    core/tagging/tagutils.cpp

    database/database.cpp
    database/library.cpp
    database/librarydatabase.cpp
    database/module.cpp
    database/playlistdatabase.cpp
    database/query.cpp

    gui/mainwindow.cpp
    gui/editablelayout.cpp
    gui/quicksetupdialog.cpp

    gui/controls/controlwidget.cpp
    gui/controls/playercontrol.cpp
    gui/controls/playlistcontrol.cpp
    gui/controls/progresswidget.cpp
    gui/controls/volumecontrol.cpp

    gui/filter/filterdelegate.cpp
    gui/filter/filteritem.cpp
    gui/filter/filtermanager.cpp
    gui/filter/filtermodel.cpp
    gui/filter/filterview.cpp
    gui/filter/filterwidget.cpp

    gui/info/itemdelegate.cpp
    gui/info/infomodel.cpp
    gui/info/infoitem.cpp
    gui/info/infowidget.cpp
    gui/info/infoview.cpp

    gui/library/coverwidget.cpp
    gui/library/searchwidget.cpp
    gui/library/statuswidget.cpp

    gui/playlist/playlistview.cpp
    gui/playlist/playlistmodel.cpp
    gui/playlist/playlistitem.cpp
    gui/playlist/playlistdelegate.cpp
    gui/playlist/playlistwidget.cpp

    gui/settings/settingsdialog.cpp
    gui/settings/settingspages.cpp

    gui/widgets/clickablelabel.cpp
    gui/widgets/dummy.cpp
    gui/widgets/hovermenu.cpp
    gui/widgets/overlay.cpp
    gui/widgets/menuheader.cpp
    gui/widgets/nolibraryoverlay.cpp
    gui/widgets/slider.cpp
    gui/widgets/spacer.cpp
    gui/widgets/splitter.cpp
    gui/widgets/splitterhandle.cpp
    gui/widgets/splitterwidget.cpp
    gui/widgets/widget.cpp

    models/container.cpp
    models/libraryitem.cpp
    models/album.cpp
    models/artist.cpp
    models/disc.cpp
    models/track.cpp

    utils/coverprovider.cpp
    utils/settings.cpp
    utils/sorting.cpp
    utils/paths.cpp
    utils/utils.cpp
    utils/widgetprovider.cpp
    utils/worker.cpp
)

set(RESOURCES
    ${CMAKE_SOURCE_DIR}/data/icons.qrc
    ${CMAKE_SOURCE_DIR}/data/fonts.qrc
)

qt6_add_resources(SOURCES ${RESOURCES})

qt_add_executable(${PROJECT_NAME}
   MANUAL_FINALIZATION
   ${HEADERS}
   ${SOURCES}
   ${QM_FILES}
)

target_compile_options(${PROJECT_NAME} PRIVATE -Werror -Wall -Wextra)

target_link_libraries(${PROJECT_NAME}
   PRIVATE
   Qt${MAJOR_QT_VERSION}::Core
   Qt${MAJOR_QT_VERSION}::Widgets
   Qt${MAJOR_QT_VERSION}::Gui
   Qt${MAJOR_QT_VERSION}::Sql
   ${TAGLIB_LIBRARIES}
   ${ZLIB_LIBRARIES}
   ${LIBMPV_LIBRARIES}
   #${SDL2_LIBRARIES}
   #${FFMPEG_LIBRARIES}
)

qt_finalize_executable(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BIN})
