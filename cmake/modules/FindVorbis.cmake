# Try to find libvorbisfile
# Once done this will define
#
# Vorbis_FOUND - system has libvorbisfile
# Vorbis::VorbisFile - imported libvorbisfile target

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_Vorbis QUIET vorbisfile)
endif()

find_path(
    Vorbis_INCLUDE_DIR
    NAMES vorbis/vorbisfile.h
    HINTS ${PC_Vorbis_INCLUDEDIR} ${PC_Vorbis_INCLUDE_DIRS}
    DOC "libvorbis include directory"
)

find_library(
    VorbisFile_LIBRARY
    NAMES vorbisfile vorbisfile_static
    HINTS ${PC_Vorbis_LIBDIR} ${PC_Vorbis_LIBRARY_DIRS}
    DOC "libvorbisfile library"
)

find_library(
    Vorbis_LIBRARY
    NAMES vorbis vorbis_static
    HINTS ${PC_Vorbis_LIBDIR} ${PC_Vorbis_LIBRARY_DIRS} ${PC_Vorbis_STATIC_LIBRARY_DIRS}
    DOC "libvorbis library"
)

find_library(
    Vorbis_OGG_LIBRARY
    NAMES ogg ogg_static
    HINTS ${PC_Vorbis_LIBDIR} ${PC_Vorbis_LIBRARY_DIRS} ${PC_Vorbis_STATIC_LIBRARY_DIRS}
    DOC "libogg library"
)

set(Vorbis_VERSION ${PC_Vorbis_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Vorbis
    REQUIRED_VARS VorbisFile_LIBRARY Vorbis_LIBRARY Vorbis_OGG_LIBRARY Vorbis_INCLUDE_DIR
    VERSION_VAR Vorbis_VERSION
)

mark_as_advanced(Vorbis_INCLUDE_DIR VorbisFile_LIBRARY Vorbis_LIBRARY Vorbis_OGG_LIBRARY)

if(Vorbis_FOUND AND NOT TARGET Vorbis::VorbisFile)
    add_library(Vorbis::VorbisFile UNKNOWN IMPORTED GLOBAL)
    set_target_properties(
        Vorbis::VorbisFile
        PROPERTIES IMPORTED_LOCATION "${VorbisFile_LIBRARY}"
                   INTERFACE_COMPILE_OPTIONS "${PC_Vorbis_CFLAGS_OTHER}"
                   INTERFACE_INCLUDE_DIRECTORIES "${Vorbis_INCLUDE_DIR}"
                   INTERFACE_LINK_LIBRARIES "${Vorbis_LIBRARY};${Vorbis_OGG_LIBRARY}"
    )
endif()
