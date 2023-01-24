fooyin_add_library(PluginSystem
    TYPE SHARED
    PREFIX Yes
    DEPENDS
        Qt6::Core
    SOURCES
        plugininfo.cpp
        pluginmanager.cpp
)
