create_fooyin_plugin_internal(
    runservices
    DEPENDS Fooyin::Gui
    SOURCES runserviceexecutor.cpp
            runserviceexecutor.h
            runservices.cpp
            runservices.h
            runservicesconstants.h
            runservicespage.cpp
            runservicespage.h
            runservicesplugin.cpp
            runservicesplugin.h
)
