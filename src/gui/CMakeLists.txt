cmake_minimum_required(VERSION 3.14)

project(
    fooyin_gui
    VERSION 0.1.0
    DESCRIPTION "Gui library for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(HEADERS
    ${CMAKE_SOURCE_DIR}/include/gui/guisettings.h
)

set(SOURCES
    fywidget.cpp
    guiapplication.cpp
    mainmenubar.cpp
    mainwindow.cpp
    editablelayout.cpp
    guipaths.cpp
    internalguisettings.cpp
    layoutprovider.cpp
    trackselectioncontroller.cpp
    widgetfilter.cpp
    widgetprovider.cpp
    controls/controlwidget.cpp
    controls/playercontrol.cpp
    controls/playlistcontrol.cpp
    controls/progresswidget.cpp
    controls/volumecontrol.cpp
    dialog/propertiesdialog.cpp
    info/infodelegate.cpp
    info/infomodel.cpp
    info/infowidget.cpp
    library/coverprovider.cpp
    library/coverwidget.cpp
    library/statuswidget.cpp
    librarytree/librarytreegroupregistry.cpp
    librarytree/librarytreeitem.cpp
    librarytree/librarytreemodel.cpp
    librarytree/librarytreepopulator.cpp
    librarytree/librarytreeview.cpp
    librarytree/librarytreewidget.cpp
    menu/aboutdialog.cpp
    menu/editmenu.cpp
    menu/filemenu.cpp
    menu/helpmenu.cpp
    menu/librarymenu.cpp
    menu/playbackmenu.cpp
    menu/viewmenu.cpp
    playlist/playlistcolumnregistry.cpp
    playlist/playlistcontroller.cpp
    playlist/playlistview.cpp
    playlist/playlistmodel.cpp
    playlist/playlistmodel_p.cpp
    playlist/playlistitem.cpp
    playlist/playlistitemmodels.cpp
    playlist/playlistdelegate.cpp
    playlist/playlisthistory.cpp
    playlist/playlistpopulator.cpp
    playlist/playlistpreset.cpp
    playlist/playlisttabs.cpp
    playlist/playlistwidget.cpp
    playlist/presetregistry.cpp
    playlist/playlistscriptregistry.cpp
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
    settings/librarytree/librarytreepage.cpp
    settings/librarytree/librarytreeguipage.cpp
    settings/librarytree/librarytreegroupmodel.cpp
    settings/playlist/playlistcolumnmodel.cpp
    settings/playlist/playlistcolumnpage.cpp
    settings/playlist/playlistgeneralpage.cpp
    settings/playlist/playlistguipage.cpp
    settings/playlist/playlistpresetspage.cpp
    settings/shortcuts/shortcutsmodel.cpp
    settings/shortcuts/shortcutspage.cpp
    settings/widgets/statuswidgetpage.cpp
    widgets/dummy.cpp
    widgets/spacer.cpp
    widgets/customisableinput.cpp
    widgets/splitterwidget.cpp
    widgets/tabstackwidget.cpp
    widgets/widgetcontainer.cpp
)

create_fooyin_library(
    fooyin_gui
    EXPORT_NAME Gui
    SOURCES ${HEADERS} ${SOURCES}
)

target_link_libraries(
    fooyin_gui
    PUBLIC Fooyin::Core
           Fooyin::Utils
    PRIVATE Fooyin::CorePrivate
)
