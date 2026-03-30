create_fooyin_plugin_internal(
    fileops
    DEPENDS Fooyin::CorePrivate
            Fooyin::Gui
    SOURCES fileopsconfigdialog.cpp
            fileopsconfigdialog.h
            fileopsdialog.cpp
            fileopsdialog.h
            fileopsdeletedialog.cpp
            fileopsdeletedialog.h
            fileopsmodel.cpp
            fileopsmodel.h
            fileopsplugin.cpp
            fileopsplugin.h
            fileopssettings.cpp
            fileopssettings.h
            fileopsworker.cpp
            fileopsworker.h
)
