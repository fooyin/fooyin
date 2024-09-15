create_fooyin_plugin_internal(
    tageditor
    DEPENDS Fooyin::Gui
    SOURCES tageditorconstants.h
            tageditoritem.cpp
            tageditoritem.h
            tageditormodel.cpp
            tageditormodel.h
            tageditorplugin.cpp
            tageditorplugin.h
            tageditorview.cpp
            tageditorview.h
            tageditorwidget.cpp
            tageditorwidget.h
            settings/tageditorfieldregistry.cpp
            settings/tageditorfieldregistry.h
            settings/tageditorfieldsmodel.cpp
            settings/tageditorfieldsmodel.h
            settings/tageditorpage.cpp
            settings/tageditorpage.h
)
