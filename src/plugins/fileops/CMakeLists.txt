create_fooyin_plugin_internal(
    fileops
    DEPENDS Fooyin::CorePrivate
            Fooyin::Gui
    SOURCES fileopsdialog.cpp
            fileopsdialog.h
            fileopsmodel.cpp
            fileopsmodel.h
            fileopsplugin.cpp
            fileopsplugin.h
            fileopsregistry.cpp
            fileopsregistry.h
            fileopssettings.cpp
            fileopssettings.h
            fileopsworker.cpp
            fileopsworker.h
)
