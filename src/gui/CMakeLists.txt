fooyin_add_library(Gui
    TYPE SHARED
    PREFIX Yes
    DEPENDS
        Qt6::Core
        Qt6::Widgets
        Fooyin_Core
        Fooyin_Utils
    SOURCES
        fywidget.h
        fywidget.cpp
        mainmenubar.h
        mainmenubar.cpp
        mainwindow.h
        mainwindow.cpp
        editablelayout.h
        editablelayout.cpp
        guisettings.h
        guisettings.cpp
        layoutprovider.h
        layoutprovider.cpp
        widgetfactory.h
        widgetprovider.h
        widgetprovider.cpp

        controls/controlwidget.h
        controls/controlwidget.cpp
        controls/playercontrol.h
        controls/playercontrol.cpp
        controls/playlistcontrol.h
        controls/playlistcontrol.cpp
        controls/progresswidget.h
        controls/progresswidget.cpp
        controls/volumecontrol.h
        controls/volumecontrol.cpp

        info/itemdelegate.h
        info/itemdelegate.cpp
        info/infomodel.h
        info/infomodel.cpp
        info/infoitem.h
        info/infoitem.cpp
        info/infowidget.h
        info/infowidget.cpp
        info/infoview.h
        info/infoview.cpp

        library/coverwidget.h
        library/coverwidget.cpp
        library/statuswidget.h
        library/statuswidget.cpp

        playlist/playlistview.h
        playlist/playlistview.cpp
        playlist/playlistmodel.h
        playlist/playlistmodel.cpp
        playlist/playlistitem.h
        playlist/playlistitem.cpp
        playlist/playlistdelegate.h
        playlist/playlistdelegate.cpp
        playlist/playlistwidget.h
        playlist/playlistwidget.cpp

        quicksetup/quicksetupdialog.h
        quicksetup/quicksetupdialog.cpp
        quicksetup/quicksetupmodel.h
        quicksetup/quicksetupmodel.cpp

        settings/settingsdialog.h
        settings/settingsdialog.cpp
        settings/settingspages.h
        settings/settingspages.cpp

        widgets/clickablelabel.h
        widgets/clickablelabel.cpp
        widgets/dummy.h
        widgets/dummy.cpp
        widgets/hovermenu.h
        widgets/hovermenu.cpp
        widgets/comboicon.h
        widgets/comboicon.cpp
        widgets/overlayfilter.h
        widgets/overlayfilter.cpp
        widgets/menuheader.h
        widgets/menuheader.cpp
        widgets/overlaywidget.h
        widgets/overlaywidget.cpp
        widgets/slider.h
        widgets/slider.cpp
        widgets/spacer.h
        widgets/spacer.cpp
        widgets/splitter.h
        widgets/splitter.cpp
        widgets/splitterhandle.h
        widgets/splitterhandle.cpp
        widgets/splitterwidget.h
        widgets/splitterwidget.cpp
)
