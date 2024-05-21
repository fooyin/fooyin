find_package(ALSA QUIET)

if(NOT ALSA_FOUND)
    return()
endif()

add_definitions(-DBUILD_ALSA_ENABLED)

create_fooyin_plugin_internal(
    alsa
    DEPENDS Fooyin::Core
            ${ALSA_LIBRARIES}
    SOURCES alsaoutput.cpp
            alsaoutput.h
            alsaplugin.cpp
            alsaplugin.h
)

target_include_directories(
    alsa PRIVATE ${ALSA_INCLUDE_DIRS}
)
