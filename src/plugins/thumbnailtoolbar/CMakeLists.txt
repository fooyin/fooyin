message(STATUS "On Windows, enabling thumbnailtoolbar plugin")

set(SOURCES
    thumbnailtoolbarplugin.cpp
    thumbnailtoolbarplugin.h
)

create_fooyin_plugin_internal(
    thumbnailtoolbar
    DEPENDS Fooyin::Gui
    SOURCES ${SOURCES}
)
