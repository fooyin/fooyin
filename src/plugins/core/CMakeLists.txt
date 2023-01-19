fooyin_add_plugin(Core
    TYPE SHARED
    DEPENDS
        Qt6::Core
        Qt6::Widgets
        Qt6::Sql
        FooyinVersion
        PluginSystem
        Utils
    SOURCES
        coreplugin.h
        coreplugin.cpp
        coresettings.h
        coresettings.cpp
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
        library/musiclibraryinteractor.h
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

        settings/settingsmanager.h
        settings/settingsmanager.cpp
        settings/settingsentry.h
        settings/settingsentry.cpp
        settings/settingtypes.h

        tagging/tags.h
        tagging/tags.cpp
        tagging/tagutils.h
        tagging/tagutils.cpp
)
