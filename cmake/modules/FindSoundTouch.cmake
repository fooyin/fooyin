# Try to find SoundTouch
# Once done this will define
#
# SoundTouch_FOUND - system has SoundTouch
# SoundTouch::SoundTouch - the SoundTouch library

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(PC_SoundTouch QUIET soundtouch)
endif ()

find_path(
        SoundTouch_INCLUDE_DIR
        NAMES SoundTouch.h
        HINTS ${PC_SoundTouch_INCLUDE_DIRS}
        PATH_SUFFIXES soundtouch
        DOC "SoundTouch include directory"
)
mark_as_advanced(SoundTouch_INCLUDE_DIR)

find_library(
        SoundTouch_LIBRARY
        NAMES SoundTouch soundtouch
        HINTS ${PC_SoundTouch_LIBRARY_DIRS}
        DOC "SoundTouch library"
)
mark_as_advanced(SoundTouch_LIBRARY)

if (DEFINED PC_SoundTouch_VERSION AND NOT PC_SoundTouch_VERSION STREQUAL "")
    set(SoundTouch_VERSION "${PC_SoundTouch_VERSION}")
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
        SoundTouch
        REQUIRED_VARS SoundTouch_LIBRARY SoundTouch_INCLUDE_DIR
        VERSION_VAR SoundTouch_VERSION
)

if (SoundTouch_FOUND)
    set(SoundTouch_LIBRARIES "${SoundTouch_LIBRARY}")
    set(SoundTouch_INCLUDE_DIRS "${SoundTouch_INCLUDE_DIR}")
    set(SoundTouch_DEFINITIONS ${PC_SoundTouch_CFLAGS_OTHER})

    if (NOT TARGET SoundTouch::SoundTouch)
        add_library(SoundTouch::SoundTouch UNKNOWN IMPORTED)
        set_target_properties(
                SoundTouch::SoundTouch
                PROPERTIES IMPORTED_LOCATION "${SoundTouch_LIBRARY}"
                INTERFACE_COMPILE_OPTIONS "${PC_SoundTouch_CFLAGS_OTHER}"
                INTERFACE_INCLUDE_DIRECTORIES "${SoundTouch_INCLUDE_DIR}"
        )
    endif ()
endif ()
