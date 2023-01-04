fooyin_add_plugin(Filters
    TYPE SHARED
    PLUGIN_DEPENDS FooyinVersion PluginSystem Core Utils
    DEPENDS Qt6::Core Qt6::Widgets Qt6::Sql
    SOURCES
        filtersplugin.h
        filtersplugin.cpp
        filterdatabase.h
        filterdatabase.cpp
        filterdatabasemanager.h
        filterdatabasemanager.cpp
        filterdelegate.h
        filterdelegate.cpp
        filteritem.h
        filteritem.cpp
        filtermanager.h
        filtermanager.cpp
        filtermodel.h
        filtermodel.cpp
        filterview.h
        filterview.cpp
        filterwidget.h
        filterwidget.cpp
        filterfwd.h
)
