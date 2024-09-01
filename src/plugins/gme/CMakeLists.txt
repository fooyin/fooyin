find_package(LIBGME QUIET)

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

target_link_directories(
    gmeinput
    PRIVATE ${LIBGME_LIBRARY_DIRS}
)

fooyin_append_mimes(
    audio/x-ay
    audio/x-gbs
    audio/x-hes
    audio/x-kss
    audio/x-nsf
    audio/x-nsfe
    audio/x-sap
    audio/x-spc
)
