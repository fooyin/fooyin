# Try to find openmpt
# Once done this will define
#
# OPENMPT_FOUND - system has openmpt
# OPENMPT_INCLUDE_DIRS - the openmpt include directory
# OPENMPT_LIBRARIES - The openmpt libraries

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_OPENMPT libopenmpt QUIET)
endif()

find_path(OPENMPT_INCLUDE_DIRS libopenmpt/libopenmpt.h
                               PATHS ${PC_OPENMPT_INCLUDEDIR})
find_path(OPENMPT_INCLUDE_CONFIG libopenmpt_config.h
                                 PATH_SUFFIXES libopenmpt
                                 PATHS ${PC_OPENMPT_INCLUDEDIR})
list(APPEND OPENMPT_INCLUDE_DIRS ${OPENMPT_INCLUDE_CONFIG})
find_library(OPENMPT_LIBRARIES NAMES openmpt
                               PATHS ${PC_OPENMPT_LIBDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenMpt DEFAULT_MSG OPENMPT_INCLUDE_DIRS OPENMPT_LIBRARIES)

mark_as_advanced(OPENMPT_INCLUDE_DIRS OPENMPT_LIBRARIES)
