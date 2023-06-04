cmake_minimum_required(VERSION 3.14)

project(
    fooyin_gui
    VERSION 0.1.0
    DESCRIPTION "Gui library for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(SOURCES
    fywidget.h
    fywidget.cpp
    mainmenubar.h
    mainmenubar.cpp
    mainwindow.h
    mainwindow.cpp
    editablelayout.h
    editablelayout.cpp
    guipaths.h
    guipaths.cpp
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

    menu/aboutdialog.h
    menu/aboutdialog.cpp
    menu/editmenu.h
    menu/editmenu.cpp
    menu/filemenu.h
    menu/filemenu.cpp
    menu/helpmenu.h
    menu/helpmenu.cpp
    menu/librarymenu.h
    menu/librarymenu.cpp
    menu/playbackmenu.h
    menu/playbackmenu.cpp
    menu/viewmenu.h
    menu/viewmenu.cpp

    playlist/playlistcontroller.cpp
    playlist/playlistview.h
    playlist/playlistview.cpp
    playlist/playlistmodel.h
    playlist/playlistmodel.cpp
    playlist/playlistitem.h
    playlist/playlistitem.cpp
    playlist/playlistdelegate.h
    playlist/playlistdelegate.cpp
    playlist/playlisttabs.cpp
    playlist/playlistwidget.h
    playlist/playlistwidget.cpp

    quicksetup/quicksetupdialog.h
    quicksetup/quicksetupdialog.cpp
    quicksetup/quicksetupmodel.h
    quicksetup/quicksetupmodel.cpp

    settings/generalpage.h
    settings/generalpage.cpp
    settings/guigeneralpage.h
    settings/guigeneralpage.cpp
    settings/librarygeneralpage.h
    settings/librarygeneralpage.cpp
    settings/librarymodel.h
    settings/librarymodel.cpp
    settings/libraryitem.h
    settings/libraryitem.cpp
    settings/libraryview.h
    settings/libraryview.cpp
    settings/playlistguipage.h
    settings/playlistguipage.cpp
    settings/pluginitem.h
    settings/pluginitem.cpp
    settings/pluginsmodel.h
    settings/pluginsmodel.cpp
    settings/pluginspage.h
    settings/pluginspage.cpp

    widgets/dummy.h
    widgets/dummy.cpp
    widgets/spacer.h
    widgets/spacer.cpp
    widgets/splitter.h
    widgets/splitter.cpp
    widgets/splitterhandle.h
    widgets/splitterhandle.cpp
    widgets/splitterwidget.h
    widgets/splitterwidget.cpp
)

add_library(
    fooyin_gui
    ${FOOYIN_LIBRARY_TYPE}
    ${SOURCES}
)
add_library(fooyin::gui ALIAS fooyin_gui)

target_include_directories(fooyin_gui
  PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/src"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
)

set_target_properties(fooyin_gui PROPERTIES
    VERSION "${PROJECT_VERSION}"
    EXPORT_NAME fooyin_gui
    OUTPUT_NAME fooyin_gui
    LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_LIBRARY_OUTPUT_DIR}
    INSTALL_RPATH ${FOOYIN_LIBRARY_INSTALL_PATH}
)

target_link_libraries(fooyin_gui
   PRIVATE
     Qt6::Core
     Qt6::Widgets
     Qt6::Sql
     fooyin::core
     fooyin::utils
)

target_compile_features(fooyin_gui PUBLIC cxx_std_17)
target_compile_options(fooyin_gui PRIVATE -Werror -Wall -Wextra -Wpedantic)

install(TARGETS fooyin_gui LIBRARY DESTINATION ${FOOYIN_LIBRARY_DIR})
