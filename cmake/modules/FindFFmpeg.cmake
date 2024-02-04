# vim: ts=2 sw=2
# - Try to find the required ffmpeg components(default: AVFORMAT, AVUTIL, AVCODEC)
#
# Once done this will define
#  FFMPEG_FOUND         - System has the all required components.
#  FFMPEG_INCLUDE_DIRS  - Include directory necessary for using the required components headers.
#  FFMPEG_LIBRARIES     - Link these to use the required ffmpeg components.
#  FFMPEG_DEFINITIONS   - Compiler switches required for using the required ffmpeg components.
#
# For each of the components it will additionally set.
#   - AVCODEC
#   - AVDEVICE
#   - AVFORMAT
#   - AVUTIL
#   - POSTPROCESS
#   - SWSCALE
# the following variables will be defined
#  <component>_FOUND        - System has <component>
#  <component>_INCLUDE_DIRS - Include directory necessary for using the <component> headers
#  <component>_LIBRARIES    - Link these to use <component>
#  <component>_DEFINITIONS  - Compiler switches required for using <component>
#  <component>_VERSION      - The components version
#
# Copyright (c) 2006, Matthias Kretz, <kretz@kde.org>
# Copyright (c) 2008, Alexander Neundorf, <neundorf@kde.org>
# Copyright (c) 2011, Michael Jansen, <kde@michael-jansen.biz>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(FindPackageHandleStandardArgs)

# Taken from https://stackoverflow.com/a/32751261/1495627
# _combine_targets_property(VAR PROP target1 target2 ...)
# Helper function: Collects @PROP properties (as lists) from @target1, @target2 ..,
# combines these lists into one and store into variable @VAR.
function(_combine_targets_property VAR PROP)
    set(values) # Resulted list
    foreach(t ${ARGN})
        get_property(v TARGET ${t} PROPERTY ${PROP})
        list(APPEND values ${v})
    endforeach()
    set(${VAR} ${values} PARENT_SCOPE)
endfunction()

# imported_link_libraries(t_dest target1 target2 ...)
# Make imported library target @t_dest effectively linked with @target1, @target2 ...
function(imported_link_libraries t_dest)
    target_link_libraries(${t_dest} INTERFACE ${ARGN})
endfunction()

# The default components were taken from a survey over other FindFFMPEG.cmake files
if (NOT FFmpeg_FIND_COMPONENTS)
  set(FFmpeg_FIND_COMPONENTS AVCODEC AVFORMAT AVUTIL)
endif ()

#
### Macro: set_component_found
#
# Marks the given component as found if both *_LIBRARIES AND *_INCLUDE_DIRS is present.
#
macro(set_component_found _component )
  if (${_component}_LIBRARIES AND ${_component}_INCLUDE_DIRS)
    set(${_component}_FOUND TRUE)
  endif ()
endmacro()

#
### Macro: find_component
#
# Checks for the given component by invoking pkgconfig and then looking up the libraries and
# include directories.
#
macro(find_component _component _pkgconfig _library _header)

  if(NOT WIN32)
     # use pkg-config to get the directories and then use these values
     # in the FIND_PATH() and FIND_LIBRARY() calls
     find_package(PkgConfig)
     if (PKG_CONFIG_FOUND)
       pkg_check_modules(PC_${_component} ${_pkgconfig})
     endif ()
  endif()

  find_path(${_component}_INCLUDE_DIRS ${_header}
    HINTS
      ${PC_${_component}_INCLUDEDIR}
      ${PC_${_component}_INCLUDE_DIRS}
      "${OSSIA_SDK}/ffmpeg/include"
    PATH_SUFFIXES
      ffmpeg
  )

  find_library(${_component}_LIBRARIES NAMES ${_library}
      HINTS
      ${PC_${_component}_LIBDIR}
      ${PC_${_component}_LIBRARY_DIRS}
      "${OSSIA_SDK}/ffmpeg/lib"
  )

  set(${_component}_DEFINITIONS  ${PC_${_component}_CFLAGS_OTHER} CACHE STRING "The ${_component} CFLAGS.")
  set(${_component}_VERSION      ${PC_${_component}_VERSION}      CACHE STRING "The ${_component} version number.")

  set_component_found(${_component})

  if (${_component}_LIBRARIES AND ${_component}_INCLUDE_DIRS)
    if("${${_component}_LIBRARIES}" MATCHES ".*\.so.*$")
      add_library(${_library} SHARED IMPORTED GLOBAL)
    else()
      add_library(${_library} STATIC IMPORTED GLOBAL)
    endif()
    set_target_properties(${_library} PROPERTIES
      IMPORTED_LOCATION "${${_component}_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${${_component}_INCLUDE_DIRS}"
      INTERFACE_COMPILE_DEFINITIONS "${${_component}_DEFINITIONS}"
    )
  endif()

  mark_as_advanced(
    ${_component}_INCLUDE_DIRS
    ${_component}_LIBRARIES
    ${_component}_DEFINITIONS
    ${_component}_VERSION)

endmacro()

unset(FFMPEG_LIBRARIES CACHE)
unset(FFMPEG_DEFINITIONS CACHE)
unset(FFMPEG_INCLUDE_DIRS CACHE)
unset(FFMPEG_TARGETS CACHE)

# Check for all possible component.
find_component(AVCODEC     libavcodec     avcodec     libavcodec/avcodec.h)
find_component(AVFORMAT    libavformat    avformat    libavformat/avformat.h)
find_component(AVDEVICE    libavdevice    avdevice    libavdevice/avdevice.h)
find_component(AVUTIL      libavutil      avutil      libavutil/avutil.h)
find_component(AVFILTER    libavfilter    avfilter    libavfilter/avfilter.h)
find_component(SWSCALE     libswscale     swscale     libswscale/swscale.h)
find_component(SWRESAMPLE  libswresample  swresample  libswresample/swresample.h)
find_component(POSTPROC    libpostproc    postproc    libpostproc/postprocess.h)

# Check if the required components were found and add their stuff to the FFMPEG_* vars.
foreach (_component ${FFmpeg_FIND_COMPONENTS})
  if (${_component}_FOUND)
    # message(STATUS "Required component ${_component} present.")
    set(FFMPEG_LIBRARIES   ${FFMPEG_LIBRARIES}   ${${_component}_LIBRARIES})
    set(FFMPEG_DEFINITIONS ${FFMPEG_DEFINITIONS} ${${_component}_DEFINITIONS})
    list(APPEND FFMPEG_INCLUDE_DIRS ${${_component}_INCLUDE_DIRS})
  else ()
    # message(STATUS "Required component ${_component} missing.")
  endif ()
endforeach ()

foreach(_lib avcodec avformat avdevice avutil swscale swresample postproc)
  if(TARGET ${_lib})
    set(FFMPEG_TARGETS ${FFMPEG_TARGETS} ${_lib})
  endif()
endforeach()

# Set-up minimal dependencies
if(TARGET avcodec)
  imported_link_libraries(avcodec avutil)
endif()
if(TARGET avformat)
  imported_link_libraries(avformat avcodec avutil)
endif()
if(TARGET avfilter)
  imported_link_libraries(avfilter avformat avcodec avutil)
endif()
if(TARGET avdevice)
  imported_link_libraries(avdevice avfilter avformat avcodec avutil)
endif()
if(TARGET swscale)
  imported_link_libraries(swscale avutil)
  if(TARGET avfilter)
    imported_link_libraries(avfilter swscale)
  endif()
endif()
if(TARGET swresample)
  imported_link_libraries(swresample avutil)
endif()
if(TARGET postproc)
  imported_link_libraries(postproc avutil)
  if(TARGET avfilter)
    imported_link_libraries(avfilter postproc)
  endif()
endif()

if(TARGET avutil)
  if(UNIX OR MSYS OR MINGW)
    if(NOT APPLE)
      find_package(ZLIB)
      if(TARGET ZLIB::ZLIB)
        imported_link_libraries(avutil "ZLIB::ZLIB")
      endif()
    endif()
  endif()

  if(WIN32)
    add_library(winbcrypt STATIC IMPORTED GLOBAL)
    if(MSVC)
      find_library(BCRYPT_LIBRARY bcrypt.lib)
      set_target_properties(winbcrypt PROPERTIES
        IMPORTED_NO_SONAME 1
        IMPORTED_LOCATION "${BCRYPT_LIBRARY}"
      )
      imported_link_libraries(avdevice shlwapi.lib)
    else()
      find_library(BCRYPT_LIBRARY libbcrypt.a HINTS ${OSSIA_SDK}/llvm/x86_64-w64-mingw32/lib)

      if(BCRYPT_LIBRARY)
        set_target_properties(winbcrypt PROPERTIES
          IMPORTED_LOCATION "${BCRYPT_LIBRARY}"
          INTERFACE_LINK_LIBRARIES "${BCRYPT_LIBRARY}"
        )
      endif()
    endif()
    imported_link_libraries(avutil winbcrypt)
  endif()
endif()
# Build the include path with duplicates removed.
if (FFMPEG_INCLUDE_DIRS)
  list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
endif ()

# cache the vars.
set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS} CACHE STRING "The FFmpeg include directories." FORCE)
set(FFMPEG_LIBRARIES    ${FFMPEG_LIBRARIES}    CACHE STRING "The FFmpeg libraries." FORCE)
set(FFMPEG_DEFINITIONS  ${FFMPEG_DEFINITIONS}  CACHE STRING "The FFmpeg cflags." FORCE)
set(FFMPEG_TARGETS      ${FFMPEG_TARGETS}  CACHE STRING "The FFmpeg targets." FORCE)

mark_as_advanced(FFMPEG_INCLUDE_DIRS
                 FFMPEG_LIBRARIES
                 FFMPEG_DEFINITIONS
                 FFMPEG_TARGETS)


# Now set the noncached _FOUND vars for the components.
foreach (_component AVCODEC AVDEVICE AVFORMAT AVUTIL POSTPROCESS SWSCALE SWRESAMPLE)
  set_component_found(${_component})
endforeach ()

# Compile the list of required vars
set(_FFmpeg_REQUIRED_VARS FFMPEG_LIBRARIES FFMPEG_INCLUDE_DIRS)
foreach (_component ${FFmpeg_FIND_COMPONENTS})
  list(APPEND _FFmpeg_REQUIRED_VARS ${_component}_LIBRARIES ${_component}_INCLUDE_DIRS)
endforeach ()

# Give a nice error message if some of the required vars are missing.
find_package_handle_standard_args(FFmpeg DEFAULT_MSG ${_FFmpeg_REQUIRED_VARS})
