set(SOURCES
    ${CMAKE_SOURCE_DIR}/include/utils/itemregistry.h
    ${CMAKE_SOURCE_DIR}/include/utils/actions/command.h
    ${CMAKE_SOURCE_DIR}/include/utils/actions/actioncontainer.h
    ${CMAKE_SOURCE_DIR}/include/utils/actions/widgetcontext.h

    clickablelabel.cpp
    crypto.cpp
    expandableinputbox.cpp
    extendabletableview.cpp
    expandingcombobox.cpp
    fileutils.cpp
    id.cpp
    multilinedelegate.cpp
    paths.cpp
    recursiveselectionmodel.cpp
    simpletreeview.cpp
    scrollarea.cpp
    slider.cpp
    utils.cpp
    worker.cpp

    actions/actioncommand.cpp
    actions/actionmanager.cpp
    actions/menucontainer.cpp
    actions/proxyaction.cpp
    actions/widgetcontext.cpp

    settings/settingsdialog.cpp
    settings/settingsdialogcontroller.cpp
    settings/settingsentry.cpp
    settings/settingsmodel.cpp
    settings/settingsmanager.cpp
    settings/settingspage.cpp

    widgets/autoheaderview.cpp
    widgets/editabletabbar.cpp
    widgets/editabletabwidget.cpp
    widgets/overlaywidget.cpp
    widgets/popuplineedit.cpp
)

create_fooyin_library(
    fooyin_utils
    EXPORT_NAME Utils
    SOURCES ${SOURCES}
)

target_link_libraries(
    fooyin_utils
    PUBLIC Qt6::Core
           Qt6::Widgets
           Qt6::Sql
           Qt6::Concurrent
)
