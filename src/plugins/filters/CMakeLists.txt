create_fooyin_plugin_internal(
    filters
    DEPENDS Fooyin::Gui
    SOURCES filtercontroller.cpp
            filtercolumnregistry.cpp
            filtersplugin.cpp
            filterdelegate.cpp
            filteritem.cpp
            filtermanager.cpp
            filtermodel.cpp
            filterpopulator.cpp
            filterwidget.cpp
            settings/filterscolumnmodel.cpp
            settings/filtersettings.cpp
            settings/filterscolumnpage.cpp
            settings/filtersgeneralpage.cpp
            settings/filtersguipage.cpp
)
