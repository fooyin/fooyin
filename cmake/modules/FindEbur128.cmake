# Try to find ebur128
# Once done this will define
#
# Ebur128_FOUND - system has ebur128
# Ebur128::Ebur128 - the ebur128 library

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_Ebur128 QUIET libebur128>=1.2.4)
endif()

find_path(
    Ebur128_INCLUDE_DIR
    NAMES ebur128.h
    HINTS ${PC_Ebur128_INCLUDE_DIRS}
    PATH_SUFFIXES ebur128
    DOC "Ebur128 include directory"
)
mark_as_advanced(Ebur128_INCLUDE_DIR)

find_library(
    Ebur128_LIBRARY
    NAMES ebur128
    HINTS ${PC_Ebur128_LIBRARY_DIRS}
    DOC "Ebur128 library"
)
mark_as_advanced(Ebur128_LIBRARY)

if(DEFINED PC_Ebur128_VERSION
   AND NOT
       PC_Ebur128_VERSION
       STREQUAL
       ""
)
    set(Ebur128_VERSION "${PC_Ebur128_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Ebur128
    REQUIRED_VARS Ebur128_LIBRARY Ebur128_INCLUDE_DIR
    VERSION_VAR Ebur128_VERSION
)

if(Ebur128_FOUND)
    set(Ebur128_LIBRARIES "${Ebur128_LIBRARY}")
    set(Ebur128_INCLUDE_DIRS "${Ebur128_INCLUDE_DIR}")
    set(Ebur128_DEFINITIONS ${PC_Ebur128_CFLAGS_OTHER})

    if(NOT TARGET Ebur128::Ebur128)
        add_library(Ebur128::Ebur128 UNKNOWN IMPORTED)
        set_target_properties(
            Ebur128::Ebur128
            PROPERTIES IMPORTED_LOCATION "${Ebur128_LIBRARY}"
                       INTERFACE_COMPILE_OPTIONS "${PC_Ebur128_CFLAGS_OTHER}"
                       INTERFACE_INCLUDE_DIRECTORIES "${Ebur128_INCLUDE_DIR}"
        )
    endif()
endif()
