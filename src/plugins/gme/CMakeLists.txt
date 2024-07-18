find_package(GME QUIET)

if(NOT LIBGME_FOUND)
    message(STATUS "LIBGME not found; skipping gme plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    gmeinput
    DEPENDS Fooyin::Core
            ${LIBGME_LIBRARIES}
    SOURCES gmedefs.h
            gmeinput.cpp
            gmeinput.h
            gmeplugin.cpp
            gmeplugin.h
            gmesettings.cpp
            gmesettings.h
)

target_include_directories(
    gmeinput
    PRIVATE ${LIBGME_INCLUDE_DIRS}
)
