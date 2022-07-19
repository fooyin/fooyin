target_sources(${PROJECT_NAME} PRIVATE
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

    scanner/libraryscanner.h
    scanner/libraryscanner.cpp

    settings/settings.h
    settings/settings.cpp

    tagging/tags.h
    tagging/tags.cpp
    tagging/tagutils.h
    tagging/tagutils.cpp

    widgets/widgetfactory.h
    widgets/widgetprovider.h
    widgets/widgetprovider.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE .)
