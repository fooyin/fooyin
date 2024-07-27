find_package(LibVgm QUIET MODULE)

if(LIBVGM_FOUND)
    message(STATUS "Using system libvgm")
else()
    message(STATUS "Using 3rd-party libvgm")
    add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/libvgm libvgm EXCLUDE_FROM_ALL)
    add_library(libvgm::vgm-player ALIAS vgm-player)
endif()

create_fooyin_plugin_internal(
    vgminput
    DEPENDS Fooyin::Core
            libvgm::vgm-player
    SOURCES vgminput.h
            vgminput.cpp
            vgminputplugin.cpp
            vgminputplugin.h
            vgminputsettings.cpp
            vgminputsettings.h
)
