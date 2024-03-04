set(SOURCES
    ${CMAKE_SOURCE_DIR}/include/gui/guisettings.h

    coverprovider.cpp
    fywidget.cpp
    guiapplication.cpp
    mainwindow.cpp
    editablelayout.cpp
    guipaths.cpp
    internalguisettings.cpp
    layoutprovider.cpp
    trackselectioncontroller.cpp
    widgetfilter.cpp
    widgetprovider.cpp

    controls/playercontrol.cpp
    controls/playlistcontrol.cpp
    controls/seekbar.cpp
    controls/volumecontrol.cpp

    dialog/aboutdialog.cpp
    dialog/propertiesdialog.cpp

    info/infodelegate.cpp
    info/infomodel.cpp
    info/infowidget.cpp

    librarytree/librarytreegroupregistry.cpp
    librarytree/librarytreeitem.cpp
    librarytree/librarytreemodel.cpp
    librarytree/librarytreepopulator.cpp
    librarytree/librarytreeview.cpp
    librarytree/librarytreewidget.cpp

    menubar/editmenu.cpp
    menubar/filemenu.cpp
    menubar/helpmenu.cpp
    menubar/librarymenu.cpp
    menubar/mainmenubar.cpp
    menubar/playbackmenu.cpp
    menubar/viewmenu.cpp

    playlist/playlistcolumnregistry.cpp
    playlist/playlistcontroller.cpp
    playlist/playlistdelegate.cpp
    playlist/playlisthistory.cpp
    playlist/playlistitem.cpp
    playlist/playlistitemmodels.cpp
    playlist/playlistmodel.cpp
    playlist/playlistmodel_p.cpp
    playlist/playlistpopulator.cpp
    playlist/playlistpreset.cpp
    playlist/playlistscriptregistry.cpp
    playlist/playlisttabs.cpp
    playlist/playlistview.cpp
    playlist/playlistwidget.cpp
    playlist/presetregistry.cpp

    playlist/organiser/playlistorganiser.cpp
    playlist/organiser/playlistorganiseritem.cpp
    playlist/organiser/playlistorganisermodel.cpp

    quicksetup/quicksetupdialog.cpp
    quicksetup/quicksetupmodel.cpp

    sandbox/expressiontreemodel.cpp
    sandbox/sandboxdialog.cpp
    sandbox/scripthighlighter.cpp

    search/searchcontroller.cpp
    search/searchwidget.cpp

    settings/enginepage.cpp
    settings/generalpage.cpp
    settings/guigeneralpage.cpp
    settings/plugins/pluginsmodel.cpp
    settings/plugins/pluginspage.cpp
    settings/library/librarygeneralpage.cpp
    settings/library/librarymodel.cpp
    settings/library/librarysortingpage.cpp
    settings/library/sortingmodel.cpp
    settings/librarytree/librarytreegroupmodel.cpp
    settings/librarytree/librarytreeguipage.cpp
    settings/librarytree/librarytreepage.cpp
    settings/playlist/playlistcolumnmodel.cpp
    settings/playlist/playlistcolumnpage.cpp
    settings/playlist/playlistgeneralpage.cpp
    settings/playlist/playlistpresetspage.cpp
    settings/shortcuts/shortcutsmodel.cpp
    settings/shortcuts/shortcutspage.cpp
    settings/widgets/statuswidgetpage.cpp

    widgets/coverwidget.cpp
    widgets/customisableinput.cpp
    widgets/dummy.cpp
    widgets/hovermenu.cpp
    widgets/logslider.cpp
    widgets/menuheader.cpp
    widgets/spacer.cpp
    widgets/splitterwidget.cpp
    widgets/statuswidget.cpp
    widgets/tabstackwidget.cpp
    widgets/toolbutton.cpp
    widgets/widgetcontainer.cpp
)

create_fooyin_library(
    fooyin_gui
    EXPORT_NAME Gui
    SOURCES ${SOURCES}
)

target_link_libraries(
    fooyin_gui
    PUBLIC Fooyin::Core
           Fooyin::Utils
    PRIVATE Fooyin::CorePrivate
            Qt::Svg
)
