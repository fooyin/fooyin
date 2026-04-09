if(APPLE AND NOT LibArchive_ROOT)
    foreach(homebrew_libarchive_root
            "/opt/homebrew/opt/libarchive"
            "/usr/local/opt/libarchive")
        if(EXISTS "${homebrew_libarchive_root}")
            set(LibArchive_ROOT "${homebrew_libarchive_root}")
            break()
        endif()
    endforeach()
endif()

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
