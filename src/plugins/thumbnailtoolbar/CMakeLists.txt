message(STATUS "On Windows, enabling thumbnailtoolbar plugin")

set(SOURCES
    #[[gersemi: sort]] #
    thumbnailtoolbarplugin.cpp
    thumbnailtoolbarplugin.h
)

create_fooyin_plugin_internal(thumbnailtoolbar DEPENDS Fooyin::Gui SOURCES ${SOURCES})
