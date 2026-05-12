find_package(Qt6 COMPONENTS DBus QUIET)

if(NOT Qt6DBus_FOUND)
    message(STATUS "Qt6::DBus not found; skipping sleepinhibitor plugin.")
    return()
endif()

set(SOURCES
    inhibitor.cpp
    inhibitor.h
    sleepinhibitorplugin.cpp
    sleepinhibitorplugin.h
    sleepinhibitorsettings.cpp
    sleepinhibitorsettings.h
)

if(UNIX AND NOT APPLE)
    list(APPEND SOURCES platforms/inhibitordbus.cpp platforms/inhibitordbus.h)
endif()

if(APPLE)
    list(APPEND SOURCES platforms/inhibitormacos.cpp platforms/inhibitormacos.h)
endif()

if(WIN32)
    list(APPEND SOURCES platforms/inhibitorwin32.cpp platforms/inhibitorwin32.h)
endif()

create_fooyin_plugin_internal(
    sleepinhibitor
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Qt6::DBus
    SOURCES ${SOURCES}
)
