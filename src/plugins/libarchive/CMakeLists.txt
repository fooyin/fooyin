create_fooyin_plugin_internal(
    libarchive
    DEPENDS Fooyin::Core
            LibArchive::LibArchive
    SOURCES libarchiveinput.cpp
            libarchiveinput.h
            libarchiveplugin.cpp
            libarchiveplugin.h
)
