fooyin_add_library(Utils
    TYPE SHARED
    PREFIX Yes
    DEPENDS
        Qt6::Core
        Qt6::Widgets
    SOURCES
        clickablelabel.cpp
        comboicon.cpp
        hovermenu.cpp
        id.cpp
        menuheader.cpp
        overlayfilter.cpp
        overlaywidget.cpp
        paths.cpp
        simplelistview.cpp
        scrollarea.cpp
        slider.cpp
        tablemodel.cpp
        threadmanager.cpp
        utils.cpp
        worker.cpp

        actions/actioncontainer.cpp
        actions/actionmanager.cpp

        settings/settingsdialog.cpp
        settings/settingsdialogcontroller.cpp
        settings/settingspage.cpp
        settings/settingsmodel.cpp
        settings/settingsmanager.h
        settings/settingsmanager.cpp
        settings/settingsentry.cpp
        settings/settingtypes.h
)
