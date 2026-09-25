# Try to find libFLAC
# Once done this will define
#
# FLAC_FOUND - system has libFLAC
# FLAC::FLAC - imported libFLAC target

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_FLAC QUIET flac)
endif()

find_path(
    FLAC_INCLUDE_DIR
    NAMES FLAC/stream_decoder.h
    HINTS ${PC_FLAC_INCLUDEDIR} ${PC_FLAC_INCLUDE_DIRS}
    DOC "libFLAC include directory"
)

find_library(
    FLAC_LIBRARY
    NAMES FLAC FLAC_static
    HINTS ${PC_FLAC_LIBDIR} ${PC_FLAC_LIBRARY_DIRS}
    DOC "libFLAC library"
)

set(FLAC_VERSION ${PC_FLAC_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    FLAC
    REQUIRED_VARS FLAC_LIBRARY FLAC_INCLUDE_DIR
    VERSION_VAR FLAC_VERSION
)

mark_as_advanced(FLAC_INCLUDE_DIR FLAC_LIBRARY)

if(FLAC_FOUND AND NOT TARGET FLAC::FLAC)
    add_library(FLAC::FLAC UNKNOWN IMPORTED GLOBAL)
    set_target_properties(
        FLAC::FLAC
        PROPERTIES IMPORTED_LOCATION "${FLAC_LIBRARY}"
                   INTERFACE_INCLUDE_DIRECTORIES "${FLAC_INCLUDE_DIR}"
    )
endif()
