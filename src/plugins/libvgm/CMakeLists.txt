find_package(LibVgm QUIET MODULE)

if(LIBVGM_FOUND)
    message(STATUS "Using system libvgm")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/libvgm/CMakeLists.txt")
    message(STATUS "Using 3rd-party libvgm")
    add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/libvgm libvgm EXCLUDE_FROM_ALL)
    add_library(libvgm::vgm-player ALIAS vgm-player)
else()
    message(STATUS "libvgm not found; skipping libvgm plugin")
    return()
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

fooyin_append_mimes(
    audio/x-dro
    audio/x-gym
    audio/x-s98
    audio/x-vgm
    audio/x-vgz
)
