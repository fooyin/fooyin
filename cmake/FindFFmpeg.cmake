#
# SPDX-FileCopyrightText: 2006 Matthias Kretz <kretz@kde.org>
# SPDX-FileCopyrightText: 2008 Alexander Neundorf <neundorf@kde.org>
# SPDX-FileCopyrightText: 2011 Michael Jansen <kde@michael-jansen.biz>
# SPDX-FileCopyrightText: 2022 Luke Taylor <LukeT1@proton.me>
#
# SPDX-License-Identifier: BSD-3-Clause

#[=======================================================================[.rst:
FindFFmpeg
----------

Find the given ffmpeg components(default: AVCODEC AVFILTER AVFORMAT AVUTIL SWSCALE).

Defines the following variables:

``FFMPEG_FOUND``
      System has all required components
``FFMPEG_INCLUDE_DIRS``
      The ffmpeg include dirs for use with target_include_directories
``FFMPEG_LIBRARIES``
      The ffmpeg libraries for use with target_link_libraries()
``FFMPEG_DEFINITIONS``
      Compiler switches required for using the required ffmpeg components

For each of the components it will additionally set:

``<component>_FOUND``
      System has the component
``<component>_INCLUDE_DIRS``
      The component include dirs
``<component>_LIBRARIES``
      The component libraries
``<component>_DEFINITIONS``
      Compiler switches required for using the required the component
``<component>_VERSION``
      The components version

#]=======================================================================]

include(FindPackageHandleStandardArgs)

# The default components were taken from a survey over other FindFFMPEG.cmake
# files
if(NOT FFmpeg_FIND_COMPONENTS)
  set(FFmpeg_FIND_COMPONENTS AVCODEC AVFILTER AVFORMAT AVUTIL SWSCALE)
endif()

# Macro: set_component_found
#
# Marks the given component as found if both *_LIBRARIES AND *_INCLUDE_DIRS is
# present.
#
macro(set_component_found _component)
  if(${_component}_LIBRARIES AND ${_component}_INCLUDE_DIRS)
    set(${_component}_FOUND TRUE)
  endif()

endmacro()

#
# Macro: find_component
#
# Checks for the given component by invoking pkgconfig and then looking up the
# libraries and include directories.
#
macro(find_component _component _pkgconfig _library _header)

  if(NOT WIN32)
    # use pkg-config to get the directories and then use these values in the
    # FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_${_component} ${_pkgconfig})
    endif()
  endif()

  find_path(
    ${_component}_INCLUDE_DIRS ${_header}
    HINTS ${PC_${_component}_INCLUDEDIR} ${PC_${_component}_INCLUDE_DIRS}
    PATH_SUFFIXES ffmpeg)

  find_library(
    ${_component}_LIBRARIES
    NAMES ${_library}
    HINTS ${PC_${_component}_LIBDIR} ${PC_${_component}_LIBRARY_DIRS})

  set(${_component}_DEFINITIONS
      ${PC_${_component}_CFLAGS_OTHER}
      CACHE STRING "The ${_component} CFLAGS.")

  set(${_component}_VERSION
      ${PC_${_component}_VERSION}
      CACHE STRING "The ${_component} version number.")

  set_component_found(${_component})

  mark_as_advanced(${_component}_INCLUDE_DIRS ${_component}_LIBRARIES
                   ${_component}_DEFINITIONS ${_component}_VERSION)

endmacro()

# Check for cached results. If there are skip the costly part.
if(NOT FFMPEG_LIBRARIES)

  find_component(AVCODEC libavcodec avcodec libavcodec/avcodec.h)
  find_component(AVFILTER libavfilter avfilter libavfilter/avfilter.h)
  find_component(AVFORMAT libavformat avformat libavformat/avformat.h)
  find_component(AVDEVICE libavdevice avdevice libavdevice/avdevice.h)
  find_component(AVUTIL libavutil avutil libavutil/avutil.h)
  find_component(SWSCALE libswscale swscale libswscale/swscale.h)
  find_component(SWRESAMPLE libswresample swresample libswresample/swresample.h)
  find_component(POSTPROC libpostproc postproc libpostproc/postprocess.h)

  # Check if the required components were found and add their stuff to the
  # FFMPEG_* vars.
  foreach(_component ${FFmpeg_FIND_COMPONENTS})
    if(${_component}_FOUND)
      set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES} ${${_component}_LIBRARIES})
      set(FFMPEG_DEFINITIONS ${FFMPEG_DEFINITIONS} ${${_component}_DEFINITIONS})
      list(APPEND FFMPEG_INCLUDE_DIRS ${${_component}_INCLUDE_DIRS})
    endif()

  endforeach()

  if(FFMPEG_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
  endif()

  set(FFMPEG_INCLUDE_DIRS
      ${FFMPEG_INCLUDE_DIRS}
      CACHE STRING "The FFmpeg include directories." FORCE)
  set(FFMPEG_LIBRARIES
      ${FFMPEG_LIBRARIES}
      CACHE STRING "The FFmpeg libraries." FORCE)
  set(FFMPEG_DEFINITIONS
      ${FFMPEG_DEFINITIONS}
      CACHE STRING "The FFmpeg cflags." FORCE)

  mark_as_advanced(FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES FFMPEG_DEFINITIONS)

endif()

# Now set the noncached _FOUND vars for the components.
foreach(
  _component
  AVCODEC
  AVDEVICE
  AVFILTER
  AVFORMAT
  AVUTIL
  POSTPROCESS
  SWSCALE)
  set_component_found(${_component})
endforeach()

set(_FFmpeg_REQUIRED_VARS FFMPEG_LIBRARIES FFMPEG_INCLUDE_DIRS)

# Compile the list of required vars
foreach(_component ${FFmpeg_FIND_COMPONENTS})
  list(APPEND _FFmpeg_REQUIRED_VARS ${_component}_LIBRARIES
       ${_component}_INCLUDE_DIRS)
endforeach()

# Give a nice error message if some of the required vars are missing.
find_package_handle_standard_args(FFmpeg DEFAULT_MSG ${_FFmpeg_REQUIRED_VARS})
