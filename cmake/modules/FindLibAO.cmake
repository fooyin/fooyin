# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLibAO
--------

Find the libao cross-platform audio output library

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``libao::libao``, if
libao has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``LIBAO_FOUND``
  True if LIBAO_INCLUDE_DIR & LIBAO_LIBRARY are found

``LIBAO_LIBRARIES``
  List of libraries when using libao.

``LIBAO_INCLUDE_DIRS``
  Where to find the libao headers.

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``LIBAO_INCLUDE_DIR``
  the libao include directory

``LIBAO_LIBRARY``
  the absolute path of the libao library
#]=======================================================================]

find_path(LIBAO_INCLUDE_DIR NAMES ao/ao.h DOC "The libao include directory")
find_library(LIBAO_LIBRARY NAMES ao DOC "The libao library")

mark_as_advanced(LIBAO_INCLUDE_DIR LIBAO_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibAO REQUIRED_VARS LIBAO_LIBRARY LIBAO_INCLUDE_DIR)

if(LIBAO_FOUND)
  set(LIBAO_INCLUDE_DIRS "${LIBAO_INCLUDE_DIR}")
  set(LIBAO_LIBRARIES "${LIBAO_LIBRARY}")
  if(NOT TARGET libao::libao)
    add_library(libao::libao UNKNOWN IMPORTED)
    set_target_properties(libao::libao PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LIBAO_INCLUDE_DIRS}")
    set_target_properties(libao::libao PROPERTIES IMPORTED_LOCATION "${LIBAO_LIBRARY}")
  endif()
endif()
