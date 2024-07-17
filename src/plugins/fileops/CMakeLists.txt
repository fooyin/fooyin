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
            fileopsworker.cpp
            fileopsworker.h
)
