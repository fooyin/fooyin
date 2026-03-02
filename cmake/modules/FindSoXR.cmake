# Try to find libsoxr
# Once done this will define
#
# SoXR_FOUND - system has libsoxr
# SoXR::SoXR - the libsoxr library

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_SoXR QUIET soxr)
endif()

find_path(
    SoXR_INCLUDE_DIR
    NAMES soxr.h
    HINTS ${PC_SoXR_INCLUDE_DIRS}
    DOC "SoXR include directory"
)
mark_as_advanced(SoXR_INCLUDE_DIR)

find_library(
    SoXR_LIBRARY
    NAMES soxr
    HINTS ${PC_SoXR_LIBRARY_DIRS}
    DOC "SoXR library"
)
mark_as_advanced(SoXR_LIBRARY)

if(DEFINED PC_SoXR_VERSION AND NOT PC_SoXR_VERSION STREQUAL "")
    set(SoXR_VERSION "${PC_SoXR_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    SoXR
    REQUIRED_VARS SoXR_LIBRARY SoXR_INCLUDE_DIR
    VERSION_VAR SoXR_VERSION
)

if(SoXR_FOUND)
    set(SoXR_LIBRARIES "${SoXR_LIBRARY}")
    set(SoXR_INCLUDE_DIRS "${SoXR_INCLUDE_DIR}")
    set(SoXR_DEFINITIONS ${PC_SoXR_CFLAGS_OTHER})

    if(NOT TARGET SoXR::SoXR)
        add_library(SoXR::SoXR UNKNOWN IMPORTED)
        set_target_properties(
            SoXR::SoXR
            PROPERTIES IMPORTED_LOCATION "${SoXR_LIBRARY}"
                       INTERFACE_COMPILE_OPTIONS "${PC_SoXR_CFLAGS_OTHER}"
                       INTERFACE_INCLUDE_DIRECTORIES "${SoXR_INCLUDE_DIR}"
        )
    endif()
endif()
