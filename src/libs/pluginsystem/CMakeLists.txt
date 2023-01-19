fooyin_add_library(PluginSystem
    TYPE SHARED
    DEPENDS
        Qt6::Core
    SOURCES
        plugininfo.cpp
        pluginmanager.cpp
)
