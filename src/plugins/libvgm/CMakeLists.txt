add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/libvgm libvgm EXCLUDE_FROM_ALL)

create_fooyin_plugin_internal(
    vgminput
    DEPENDS Fooyin::Core
            vgm-player
    SOURCES vgminput.h
            vgminput.cpp
            vgminputplugin.cpp
            vgminputplugin.h
            vgminputsettings.cpp
            vgminputsettings.h
)
