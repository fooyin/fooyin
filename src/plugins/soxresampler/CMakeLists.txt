find_package(SoXR QUIET)

if(NOT SoXR_FOUND)
    message(STATUS "SoXR not found; skipping soxresampler plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    soxresampler
    DEPENDS Fooyin::Core
            Fooyin::Gui
            SoXR::SoXR
    SOURCES soxresamplerdsp.cpp
            soxresamplerdsp.h
            soxresamplerplugin.cpp
            soxresamplerplugin.h
            soxresamplersettingswidget.cpp
            soxresamplersettingswidget.h
)
