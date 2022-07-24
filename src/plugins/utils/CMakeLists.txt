fooyin_add_plugin(Utils
    TYPE SHARED
    PLUGIN_DEPENDS FooyinVersion PluginSystem
    DEPENDS Qt6::Core Qt6::Widgets
    SOURCES
        enumhelper.h
        helpers.h
        paths.h
        paths.cpp
        typedefs.h
        utils.h
        utils.cpp
)
