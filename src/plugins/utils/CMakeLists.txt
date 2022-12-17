fooyin_add_plugin(Utils
    TYPE SHARED
    PLUGIN_DEPENDS FooyinVersion PluginSystem
    DEPENDS Qt6::Core Qt6::Widgets
    SOURCES
        enumhelper.h
        helpers.h
        id.h
        id.cpp
        paths.h
        paths.cpp
        utils.h
        utils.cpp
)
