cmake_minimum_required(VERSION 3.14)

project(
    fooyin_utils
    VERSION 0.2.0
    DESCRIPTION "fooyin_utils library for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(HEADERS
    ${CMAKE_SOURCE_DIR}/include/utils/itemregistry.h
    ${CMAKE_SOURCE_DIR}/include/utils/actions/command.h
    ${CMAKE_SOURCE_DIR}/include/utils/actions/actioncontainer.h
    ${CMAKE_SOURCE_DIR}/include/utils/actions/widgetcontext.h
)

set(SOURCES
    clickablelabel.cpp
    crypto.cpp
    expandableinputbox.cpp
    expandingcombobox.cpp
    extendabletableview.cpp
    fileutils.cpp
    hovermenu.cpp
    id.cpp
    logslider.cpp
    menuheader.cpp
    multilinedelegate.cpp
    paths.cpp
    recursiveselectionmodel.cpp
    simpletreeview.cpp
    scrollarea.cpp
    slider.cpp
    utils.cpp
    worker.cpp
    actions/actioncommand.cpp
    actions/menucontainer.cpp
    actions/widgetcontext.cpp
    actions/actionmanager.cpp
    actions/proxyaction.cpp
    settings/settingsdialog.cpp
    settings/settingsdialogcontroller.cpp
    settings/settingspage.cpp
    settings/settingsmodel.cpp
    settings/settingsmanager.cpp
    settings/settingsentry.cpp
    widgets/autoheaderview.cpp
    widgets/editabletabbar.cpp
    widgets/editabletabwidget.cpp
    widgets/overlaywidget.cpp
    widgets/popuplineedit.cpp
)

create_fooyin_library(
    fooyin_utils
    EXPORT_NAME Utils
    SOURCES ${HEADERS} ${SOURCES}
)

target_link_libraries(
    fooyin_utils
    PUBLIC Qt6::Core
           Qt6::Widgets
           Qt6::Sql
           Qt6::Concurrent
)
