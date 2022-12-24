fooyin_add_plugin(Core
    TYPE SHARED
    PLUGIN_DEPENDS FooyinVersion PluginSystem Utils
    DEPENDS Qt6::Core Qt6::Widgets Qt6::Sql
    SOURCES
        coreplugin.h
        coreplugin.cpp
        constants.h
        typedefs.h

        actions/actioncontainer.h
        actions/actioncontainer.cpp
        actions/actionmanager.h
        actions/actionmanager.cpp


        app/application.h
        app/application.cpp
        app/threadmanager.h
        app/threadmanager.cpp
        app/worker.h
        app/worker.cpp

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
        library/libraryinfo.h
        library/libraryinfo.cpp
        library/librarymanager.h
        library/librarymanager.cpp
        library/librarydatabasemanager.h
        library/librarydatabasemanager.cpp
        library/musiclibrary.h
        library/musiclibrary.cpp
        library/libraryscanner.h
        library/libraryscanner.cpp
        library/libraryutils.h
        library/libraryutils.cpp

        library/models/container.h
        library/models/container.cpp
        library/models/libraryitem.h
        library/models/libraryitem.cpp
        library/models/album.h
        library/models/album.cpp
        library/models/artist.h
        library/models/artist.cpp
        library/models/disc.h
        library/models/disc.cpp
        library/models/track.h
        library/models/track.cpp
        library/models/trackfwd.h

        library/sorting/sorting.h
        library/sorting/sorting.cpp
        library/sorting/sortorder.h

        playlist/libraryplaylistinterface.h
        playlist/libraryplaylistmanager.h
        playlist/libraryplaylistmanager.cpp
        playlist/playlist.h
        playlist/playlist.cpp
        playlist/playlisthandler.h
        playlist/playlisthandler.cpp

        player/playercontroller.h
        player/playercontroller.cpp
        player/playermanager.h

        settings/settings.h
        settings/settings.cpp

        tagging/tags.h
        tagging/tags.cpp
        tagging/tagutils.h
        tagging/tagutils.cpp

        widgets/widgetfactory.h
        widgets/widgetprovider.h
        widgets/widgetprovider.cpp

        gui/fywidget.h
        gui/fywidget.cpp
        gui/mainwindow.h
        gui/mainwindow.cpp
        gui/editablelayout.h
        gui/editablelayout.cpp
        gui/quicksetupdialog.h
        gui/quicksetupdialog.cpp

        gui/controls/controlwidget.h
        gui/controls/controlwidget.cpp
        gui/controls/playercontrol.h
        gui/controls/playercontrol.cpp
        gui/controls/playlistcontrol.h
        gui/controls/playlistcontrol.cpp
        gui/controls/progresswidget.h
        gui/controls/progresswidget.cpp
        gui/controls/volumecontrol.h
        gui/controls/volumecontrol.cpp

        gui/filter/filterdelegate.h
        gui/filter/filterdelegate.cpp
        gui/filter/filteritem.h
        gui/filter/filteritem.cpp
        gui/filter/filtermanager.h
        gui/filter/filtermanager.cpp
        gui/filter/filtermodel.h
        gui/filter/filtermodel.cpp
        gui/filter/filterview.h
        gui/filter/filterview.cpp
        gui/filter/filterwidget.h
        gui/filter/filterwidget.cpp
        gui/filter/filterfwd.h

        gui/info/itemdelegate.h
        gui/info/itemdelegate.cpp
        gui/info/infomodel.h
        gui/info/infomodel.cpp
        gui/info/infoitem.h
        gui/info/infoitem.cpp
        gui/info/infowidget.h
        gui/info/infowidget.cpp
        gui/info/infoview.h
        gui/info/infoview.cpp

        gui/library/coverwidget.h
        gui/library/coverwidget.cpp
        gui/library/searchwidget.h
        gui/library/searchwidget.cpp
        gui/library/statuswidget.h
        gui/library/statuswidget.cpp

        gui/playlist/playlistview.h
        gui/playlist/playlistview.cpp
        gui/playlist/playlistmodel.h
        gui/playlist/playlistmodel.cpp
        gui/playlist/playlistitem.h
        gui/playlist/playlistitem.cpp
        gui/playlist/playlistdelegate.h
        gui/playlist/playlistdelegate.cpp
        gui/playlist/playlistwidget.h
        gui/playlist/playlistwidget.cpp

        gui/settings/settingsdialog.h
        gui/settings/settingsdialog.cpp
        gui/settings/settingspages.h
        gui/settings/settingspages.cpp

        gui/widgets/clickablelabel.h
        gui/widgets/clickablelabel.cpp
        gui/widgets/dummy.h
        gui/widgets/dummy.cpp
        gui/widgets/hovermenu.h
        gui/widgets/hovermenu.cpp
        gui/widgets/overlay.h
        gui/widgets/overlay.cpp
        gui/widgets/menuheader.h
        gui/widgets/menuheader.cpp
        gui/widgets/nolibraryoverlay.h
        gui/widgets/nolibraryoverlay.cpp
        gui/widgets/slider.h
        gui/widgets/slider.cpp
        gui/widgets/spacer.h
        gui/widgets/spacer.cpp
        gui/widgets/splitter.h
        gui/widgets/splitter.cpp
        gui/widgets/splitterhandle.h
        gui/widgets/splitterhandle.cpp
        gui/widgets/splitterwidget.h
        gui/widgets/splitterwidget.cpp
)
