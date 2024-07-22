add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/libvgm libvgm EXCLUDE_FROM_ALL)

create_fooyin_plugin_internal(
    libvgminput
    DEPENDS Fooyin::Core
            vgm-player
    SOURCES libvgmplugin.cpp
            libvgmplugin.h
)
