create_fooyin_plugin_internal(
    equaliser
    DEPENDS Fooyin::Core
            Fooyin::Gui
    SOURCES equaliserdsp.cpp
            equaliserdsp.h
            equaliserplugin.cpp
            equaliserplugin.h
            equalisersettingswidget.cpp
            equalisersettingswidget.h
            supereqadapter.cpp
            supereqadapter.h
            supereq/supereq_compat.h
            supereq/paramlist.h
            supereq/supereq.h
            supereq/fft.h
)
