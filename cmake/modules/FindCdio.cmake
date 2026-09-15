# Try to find libcdio and libcdio-paranoia
# Once done this will define
#
# Cdio_FOUND - system has libcdio and libcdio-paranoia
# Cdio::Cdio - the libcdio and libcdio-paranoia libraries

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_Cdio QUIET libcdio libcdio_cdda libcdio_paranoia)
endif()

find_path(
    Cdio_INCLUDE_DIR
    NAMES cdio/cdio.h
    HINTS ${PC_Cdio_INCLUDE_DIRS}
    DOC "libcdio include directory"
)

find_library(
    Cdio_LIBRARY
    NAMES cdio
    HINTS ${PC_Cdio_LIBRARY_DIRS}
    DOC "libcdio library"
)

find_library(
    Cdio_CDDA_LIBRARY
    NAMES cdio_cdda
    HINTS ${PC_Cdio_LIBRARY_DIRS}
    DOC "libcdio CD-DA library"
)

find_library(
    Cdio_PARANOIA_LIBRARY
    NAMES cdio_paranoia
    HINTS ${PC_Cdio_LIBRARY_DIRS}
    DOC "libcdio paranoia library"
)

mark_as_advanced(Cdio_INCLUDE_DIR Cdio_LIBRARY Cdio_CDDA_LIBRARY Cdio_PARANOIA_LIBRARY)

if(DEFINED PC_Cdio_libcdio_VERSION AND NOT PC_Cdio_libcdio_VERSION STREQUAL "")
    set(Cdio_VERSION "${PC_Cdio_libcdio_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Cdio
    REQUIRED_VARS Cdio_LIBRARY Cdio_CDDA_LIBRARY Cdio_PARANOIA_LIBRARY Cdio_INCLUDE_DIR
    VERSION_VAR Cdio_VERSION
)

if(Cdio_FOUND AND NOT TARGET Cdio::Cdio)
    add_library(Cdio::Cdio INTERFACE IMPORTED)
    target_include_directories(Cdio::Cdio INTERFACE "${Cdio_INCLUDE_DIR}")
    target_link_libraries(
        Cdio::Cdio
        INTERFACE "${Cdio_PARANOIA_LIBRARY}"
                  "${Cdio_CDDA_LIBRARY}"
                  "${Cdio_LIBRARY}"
    )
endif()
