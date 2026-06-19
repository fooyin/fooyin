set(DEPENDS
    Fooyin::Core
    Fooyin::Gui
)

if(UNIX AND NOT APPLE)
    find_package(Qt6 COMPONENTS DBus QUIET)
    if(NOT Qt6DBus_FOUND)
        message(STATUS "Qt6::DBus not found; skipping sleepinhibitor plugin.")
        return()
    else()
        list(APPEND DEPENDS Qt6::DBus)
    endif()
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
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    list(APPEND DEPENDS ${CORE_FOUNDATION_FRAMEWORK} ${IOKIT_FRAMEWORK})
endif()

if(WIN32)
    list(APPEND SOURCES platforms/inhibitorwin32.cpp platforms/inhibitorwin32.h)
endif()

create_fooyin_plugin_internal(
    sleepinhibitor
    DEPENDS ${DEPENDS}
    SOURCES ${SOURCES}
)
