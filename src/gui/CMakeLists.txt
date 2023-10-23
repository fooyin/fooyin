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
    fooyin_gui
    VERSION 0.1.0
    DESCRIPTION "Gui library for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(HEADERS
    ${CMAKE_SOURCE_DIR}/include/gui/coverprovider.h
    ${CMAKE_SOURCE_DIR}/include/gui/fywidget.h
    ${CMAKE_SOURCE_DIR}/include/gui/guisettings.h
    ${CMAKE_SOURCE_DIR}/include/gui/propertiesdialog.h
    ${CMAKE_SOURCE_DIR}/include/gui/searchcontroller.h
    ${CMAKE_SOURCE_DIR}/include/gui/splitterwidget.h
    ${CMAKE_SOURCE_DIR}/include/gui/trackselectioncontroller.h
    ${CMAKE_SOURCE_DIR}/include/gui/widgetcontainer.h
    ${CMAKE_SOURCE_DIR}/include/gui/widgetprovider.h
)

set(SOURCES
    fywidget.cpp
    guiapplication.cpp
    mainmenubar.cpp
    mainwindow.cpp
    editablelayout.cpp
    guipaths.cpp
    guisettings.cpp
    layoutprovider.cpp
    trackselectioncontroller.cpp
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
    playlist/playlistcontroller.cpp
    playlist/playlistview.cpp
    playlist/playlistmodel.cpp
    playlist/playlistitem.cpp
    playlist/playlistitemmodels.cpp
    playlist/playlistdelegate.cpp
    playlist/playlistpopulator.cpp
    playlist/playlistpreset.cpp
    playlist/playlisttabs.cpp
    playlist/playlistwidget.cpp
    playlist/presetregistry.cpp
    playlist/playlistscriptregistry.cpp
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
    settings/library/libraryview.cpp
    settings/library/librarysortingpage.cpp
    settings/library/sortingmodel.cpp
    settings/librarytree/librarytreepage.cpp
    settings/librarytree/librarytreeguipage.cpp
    settings/librarytree/librarytreegroupmodel.cpp
    settings/playlist/playlistguipage.cpp
    settings/playlist/presetinput.cpp
    settings/playlist/presetinputbox.cpp
    settings/playlist/playlistpresetspage.cpp
    widgets/dummy.cpp
    widgets/spacer.cpp
    widgets/splitterwidget.cpp
    widgets/tabstackwidget.cpp
    widgets/widgetcontainer.cpp
)

create_fooyin_library(
    fooyin_gui
    EXPORT_NAME Gui
    SOURCES ${HEADERS} ${SOURCES}
)

target_link_libraries(fooyin_gui PUBLIC Fooyin::Core Fooyin::Utils)
