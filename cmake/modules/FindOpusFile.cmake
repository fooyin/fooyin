# Try to find libopusfile
# Once done this will define
#
# OpusFile_FOUND - system has libopusfile
# OpusFile::OpusFile - imported libopusfile target

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_OpusFile QUIET opusfile)
endif()

set(OpusFile_INCLUDE_HINTS ${PC_OpusFile_INCLUDEDIR} ${PC_OpusFile_INCLUDE_DIRS})
foreach(OpusFile_INCLUDE_HINT IN LISTS OpusFile_INCLUDE_HINTS)
    get_filename_component(OpusFile_INCLUDE_PARENT "${OpusFile_INCLUDE_HINT}" DIRECTORY)
    list(APPEND OpusFile_INCLUDE_HINTS "${OpusFile_INCLUDE_PARENT}")
endforeach()

find_path(
    OpusFile_ROOT_INCLUDE_DIR
    NAMES opus/opusfile.h
    HINTS ${OpusFile_INCLUDE_HINTS}
    PATH_SUFFIXES include
    DOC "libopusfile include directory"
)

find_path(
    OpusFile_OPUS_INCLUDE_DIR
    NAMES opus_multistream.h
    HINTS ${OpusFile_INCLUDE_HINTS}
    PATH_SUFFIXES opus include/opus
    DOC "libopus include directory"
)

unset(OpusFile_INCLUDE_HINT)
unset(OpusFile_INCLUDE_HINTS)
unset(OpusFile_INCLUDE_PARENT)

find_library(
    OpusFile_LIBRARY
    NAMES opusfile opusfile_static
    HINTS ${PC_OpusFile_LIBDIR} ${PC_OpusFile_LIBRARY_DIRS}
    DOC "libopusfile library"
)

find_library(
    OpusFile_OGG_LIBRARY
    NAMES ogg
    HINTS ${PC_OpusFile_LIBDIR} ${PC_OpusFile_LIBRARY_DIRS} ${PC_OpusFile_STATIC_LIBRARY_DIRS}
    DOC "libogg library"
)

find_library(
    OpusFile_OPUS_LIBRARY
    NAMES opus
    HINTS ${PC_OpusFile_LIBDIR} ${PC_OpusFile_LIBRARY_DIRS} ${PC_OpusFile_STATIC_LIBRARY_DIRS}
    DOC "libopus library"
)

set(OpusFile_VERSION ${PC_OpusFile_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    OpusFile
    REQUIRED_VARS OpusFile_LIBRARY
                  OpusFile_OGG_LIBRARY
                  OpusFile_OPUS_LIBRARY
                  OpusFile_ROOT_INCLUDE_DIR
                  OpusFile_OPUS_INCLUDE_DIR
    VERSION_VAR OpusFile_VERSION
)

mark_as_advanced(
    OpusFile_ROOT_INCLUDE_DIR
    OpusFile_OPUS_INCLUDE_DIR
    OpusFile_LIBRARY
    OpusFile_OGG_LIBRARY
    OpusFile_OPUS_LIBRARY
)

if(OpusFile_FOUND AND NOT TARGET OpusFile::OpusFile)
    add_library(OpusFile::OpusFile UNKNOWN IMPORTED GLOBAL)
    set_target_properties(
        OpusFile::OpusFile
        PROPERTIES IMPORTED_LOCATION "${OpusFile_LIBRARY}"
                   INTERFACE_INCLUDE_DIRECTORIES "${OpusFile_ROOT_INCLUDE_DIR};${OpusFile_OPUS_INCLUDE_DIR}"
                   INTERFACE_LINK_LIBRARIES "${OpusFile_OGG_LIBRARY};${OpusFile_OPUS_LIBRARY}"
    )
endif()
