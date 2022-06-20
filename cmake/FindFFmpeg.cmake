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

INCLUDE(FindPackageHandleStandardArgs)

# The default components were taken from a survey over other FindFFMPEG.cmake files
IF(NOT FFmpeg_FIND_COMPONENTS)
    SET(FFmpeg_FIND_COMPONENTS AVCODEC AVFILTER AVFORMAT AVUTIL SWSCALE)
ENDIF()

### Macro: set_component_found
#
# Marks the given component as found if both *_LIBRARIES AND *_INCLUDE_DIRS is present.
#
MACRO(set_component_found _component )
    IF(${_component}_LIBRARIES AND ${_component}_INCLUDE_DIRS)
        SET(${_component}_FOUND TRUE)
    ENDIF()

ENDMACRO()

#
### Macro: find_component
#
# Checks for the given component by invoking pkgconfig and then looking up the libraries and
# include directories.
#
MACRO(find_component _component _pkgconfig _library _header)

    IF(NOT WIN32)
        # use pkg-config to get the directories and then use these values
        # in the FIND_PATH() and FIND_LIBRARY() calls
        FIND_PACKAGE(PkgConfig)
        IF(PKG_CONFIG_FOUND)
            pkg_check_modules(PC_${_component} ${_pkgconfig})
        ENDIF()
    ENDIF()

    FIND_PATH(${_component}_INCLUDE_DIRS ${_header}
        HINTS
            ${PC_${_component}_INCLUDEDIR}
            ${PC_${_component}_INCLUDE_DIRS}
        PATH_SUFFIXES
            ffmpeg
    )

    FIND_LIBRARY(${_component}_LIBRARIES NAMES ${_library}
                 HINTS
                     ${PC_${_component}_LIBDIR}
                     ${PC_${_component}_LIBRARY_DIRS}
    )

    SET(${_component}_DEFINITIONS  ${PC_${_component}_CFLAGS_OTHER}
        CACHE STRING "The ${_component} CFLAGS.")

    SET(${_component}_VERSION      ${PC_${_component}_VERSION}
        CACHE STRING "The ${_component} version number.")

    SET_COMPONENT_FOUND(${_component})

    MARK_AS_ADVANCED(
        ${_component}_INCLUDE_DIRS
        ${_component}_LIBRARIES
        ${_component}_DEFINITIONS
        ${_component}_VERSION)

ENDMACRO()

# Check for cached results. If there are skip the costly part.
if(NOT FFMPEG_LIBRARIES)

    FIND_COMPONENT(AVCODEC      libavcodec      avcodec     libavcodec/avcodec.h)
    FIND_COMPONENT(AVFILTER     libavfilter     avfilter    libavfilter/avfilter.h)
    FIND_COMPONENT(AVFORMAT     libavformat     avformat    libavformat/avformat.h)
    FIND_COMPONENT(AVDEVICE     libavdevice     avdevice    libavdevice/avdevice.h)
    FIND_COMPONENT(AVUTIL       libavutil       avutil      libavutil/avutil.h)
    FIND_COMPONENT(SWSCALE      libswscale      swscale     libswscale/swscale.h)
    FIND_COMPONENT(SWRESAMPLE   libswresample   swresample  libswresample/swresample.h)
    FIND_COMPONENT(POSTPROC     libpostproc     postproc    libpostproc/postprocess.h)

    # Check if the required components were found and add their stuff to the FFMPEG_* vars.
    FOREACH(_component ${FFmpeg_FIND_COMPONENTS})
        IF (${_component}_FOUND)
            SET(FFMPEG_LIBRARIES   ${FFMPEG_LIBRARIES}   ${${_component}_LIBRARIES})
            SET(FFMPEG_DEFINITIONS ${FFMPEG_DEFINITIONS} ${${_component}_DEFINITIONS})
            LIST(APPEND FFMPEG_INCLUDE_DIRS ${${_component}_INCLUDE_DIRS})
        ENDIF()

    ENDFOREACH()

    IF(FFMPEG_INCLUDE_DIRS)
        LIST(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
    ENDIF()

    SET(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS} CACHE STRING "The FFmpeg include directories." FORCE)
    SET(FFMPEG_LIBRARIES    ${FFMPEG_LIBRARIES}    CACHE STRING "The FFmpeg libraries."           FORCE)
    SET(FFMPEG_DEFINITIONS  ${FFMPEG_DEFINITIONS}  CACHE STRING "The FFmpeg cflags."              FORCE)

    MARK_AS_ADVANCED(FFMPEG_INCLUDE_DIRS
                     FFMPEG_LIBRARIES
                     FFMPEG_DEFINITIONS)

ENDIF()

# Now set the noncached _FOUND vars for the components.
FOREACH(_component AVCODEC AVDEVICE AVFILTER AVFORMAT AVUTIL POSTPROCESS SWSCALE)
    SET_COMPONENT_FOUND(${_component})
ENDFOREACH()

SET(_FFmpeg_REQUIRED_VARS FFMPEG_LIBRARIES FFMPEG_INCLUDE_DIRS)

# Compile the list of required vars
FOREACH(_component ${FFmpeg_FIND_COMPONENTS})
    LIST(APPEND _FFmpeg_REQUIRED_VARS ${_component}_LIBRARIES ${_component}_INCLUDE_DIRS)
ENDFOREACH()

# Give a nice error message if some of the required vars are missing.
FIND_PACKAGE_HANDLE_STANDARD_ARGS(FFmpeg DEFAULT_MSG ${_FFmpeg_REQUIRED_VARS})
