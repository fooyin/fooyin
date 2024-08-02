find_package(LibArchive QUIET)

if(NOT LibArchive_FOUND)
    message(STATUS "libarchive not found; skipping libarchive plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    libarchive
    DEPENDS Fooyin::Core
            LibArchive::LibArchive
    SOURCES libarchiveinput.cpp
            libarchiveinput.h
            libarchiveplugin.cpp
            libarchiveplugin.h
)
