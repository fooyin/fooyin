# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLibVgm
--------

Find the libvgm library

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``libvgm::libvgm-player``, if
libvgm has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``LIBVGM_FOUND``
  True if LIBVGM_INCLUDE_DIR & LIBVGM_LIBRARY are found

``LIBVGM_LIBRARIES``
  List of libraries when using libvgm.

``LIBVGM_INCLUDE_DIRS``
  Where to find the libvgm headers.

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``LIBVGM_INCLUDE_DIR``
  the libvgm include directory

``LIBVGM_LIBRARY``
  the absolute path of the libvgm library
#]=======================================================================]

find_path(LIBVGM_INCLUDE_DIR NAMES vgm DOC "The libvgm include directory")
find_library(LIBVGM_LIBRARY NAMES vgm-player DOC "The libvgm library")

mark_as_advanced(LIBVGM_INCLUDE_DIR LIBVGM_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibVgm REQUIRED_VARS LIBVGM_LIBRARY LIBVGM_INCLUDE_DIR)

if(LIBVGM_FOUND)
  set(LIBVGM_INCLUDE_DIRS "${LIBVGM_INCLUDE_DIR}/vgm")
  set(LIBVGM_LIBRARIES "${LIBVGM_LIBRARY}")
  if(NOT TARGET libvgm::vgm-player)
    add_library(libvgm::vgm-player UNKNOWN IMPORTED)
    set_target_properties(libvgm::vgm-player PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LIBVGM_INCLUDE_DIRS}")
    set_target_properties(libvgm::vgm-player PROPERTIES IMPORTED_LOCATION "${LIBVGM_LIBRARY}")
  endif()
endif()
