target_sources(${PROJECT_NAME} PRIVATE
    app/application.h
    app/application.cpp
    app/threadmanager.h
    app/threadmanager.cpp

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

    library/libraryinfo.h
    library/libraryinfo.cpp
    library/librarymanager.h
    library/librarymanager.cpp
    library/librarydatabasemanager.h
    library/librarydatabasemanager.cpp
    library/musiclibrary.h
    library/musiclibrary.cpp

    models/container.h
    models/container.cpp
    models/libraryitem.h
    models/libraryitem.cpp
    models/album.h
    models/album.cpp
    models/artist.h
    models/artist.cpp
    models/disc.h
    models/disc.cpp
    models/track.h
    models/track.cpp

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

    tagging/tags.h
    tagging/tags.cpp
    tagging/tagutils.h
    tagging/tagutils.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE .)
