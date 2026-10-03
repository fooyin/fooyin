# Try to find JACK
# Once done this will define
#
# Jack_FOUND - system has JACK
# Jack::Jack - imported JACK target

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_Jack QUIET jack)
endif()

find_path(
    Jack_INCLUDE_DIR
    NAMES jack/jack.h
    HINTS ${PC_Jack_INCLUDE_DIRS}
    DOC "JACK include directory"
)

find_library(
    Jack_LIBRARY
    NAMES jack
    HINTS ${PC_Jack_LIBRARY_DIRS}
    DOC "JACK library"
)

set(Jack_VERSION "${PC_Jack_VERSION}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Jack
    REQUIRED_VARS Jack_LIBRARY Jack_INCLUDE_DIR
    VERSION_VAR Jack_VERSION
)

mark_as_advanced(Jack_INCLUDE_DIR Jack_LIBRARY)

if(Jack_FOUND AND NOT TARGET Jack::Jack)
    add_library(Jack::Jack UNKNOWN IMPORTED)
    set_target_properties(
        Jack::Jack
        PROPERTIES IMPORTED_LOCATION "${Jack_LIBRARY}"
                   INTERFACE_COMPILE_OPTIONS "${PC_Jack_CFLAGS_OTHER}"
                   INTERFACE_INCLUDE_DIRECTORIES "${Jack_INCLUDE_DIR}"
    )
endif()
