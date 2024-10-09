create_fooyin_plugin_internal(
    rgscanner
    DEPENDS Fooyin::CorePrivate
            Fooyin::Gui
    SOURCES ffmpegscanner.cpp
            ffmpegscanner.h
            rgscanner.cpp
            rgscanner.h
            rgscanresults.cpp
            rgscanresults.h
            rgscanresultsmodel.cpp
            rgscanresultsmodel.h
            rgscannerplugin.cpp
            rgscannerplugin.h
)
