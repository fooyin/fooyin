create_fooyin_plugin_internal(
    quicktagger
    DEPENDS Fooyin::Gui
    SOURCES quicktagger.cpp
            quicktagger.h
            quicktaggerconstants.h
            quicktaggermodel.cpp
            quicktaggermodel.h
            quicktaggerpage.cpp
            quicktaggerpage.h
            quicktaggerplugin.cpp
            quicktaggerplugin.h
)
