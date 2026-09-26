# Try to find libmpg123
# Once done this will define
#
# Mpg123_FOUND - system has libmpg123
# Mpg123::Mpg123 - imported libmpg123 target

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_Mpg123 QUIET libmpg123)
endif()

find_path(
    Mpg123_INCLUDE_DIR
    NAMES mpg123.h
    HINTS ${PC_Mpg123_INCLUDE_DIRS}
    DOC "libmpg123 include directory"
)

find_library(
    Mpg123_LIBRARY
    NAMES mpg123 libmpg123
    HINTS ${PC_Mpg123_LIBRARY_DIRS}
    DOC "libmpg123 library"
)

set(Mpg123_VERSION ${PC_Mpg123_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Mpg123
    REQUIRED_VARS Mpg123_LIBRARY Mpg123_INCLUDE_DIR
    VERSION_VAR Mpg123_VERSION
)

mark_as_advanced(Mpg123_INCLUDE_DIR Mpg123_LIBRARY)

if(Mpg123_FOUND AND NOT TARGET Mpg123::Mpg123)
    add_library(Mpg123::Mpg123 UNKNOWN IMPORTED GLOBAL)
    set_target_properties(
        Mpg123::Mpg123
        PROPERTIES IMPORTED_LOCATION "${Mpg123_LIBRARY}"
                   INTERFACE_COMPILE_OPTIONS "${PC_Mpg123_CFLAGS_OTHER}"
                   INTERFACE_INCLUDE_DIRECTORIES "${Mpg123_INCLUDE_DIR}"
    )
endif()
