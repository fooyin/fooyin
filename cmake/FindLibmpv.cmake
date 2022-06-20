#
# SPDX-FileCopyrightText: 2006 Laurent Montel <montel@kde.org>
# SPDX-FileCopyrightText: 2019 Heiko Becker <heirecka@exherbo.org>
# SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
# SPDX-FileCopyrightText: 2022 Luke Taylor <LukeT1@proton.me>
#
# SPDX-License-Identifier: BSD-3-Clause

#[=======================================================================[.rst:
FindLibmpv
----------

Find the mpv media player client library.

Defines the following variables:

``LIBMPV_FOUND``
      True if the system has the libmpv library of at least the minimum
      version specified by the version parameter to find_package()
``LIBMPV_INCLUDE_DIRS``
      The libmpv include dirs for use with target_include_directories
``LIBMPV_LIBRARIES``
      The libmpv libraries for use with target_link_libraries()
``LIBMPV_VERSION``
      The version of libmpv that was found

If ``LIBMPV_FOUND`` is TRUE, it will also define the following imported
target:

``Libmpv::Libmpv``

#]=======================================================================]

find_package(PkgConfig QUIET)

pkg_search_module(PC_MPV QUIET mpv)

find_path(LIBMPV_INCLUDE_DIRS
    NAMES client.h
    PATH_SUFFIXES mpv
    HINTS ${PC_MPV_INCLUDEDIR}
)


find_library(LIBMPV_LIBRARIES
    NAMES mpv
    HINTS ${PC_MPV_LIBDIR}
)

set(LIBMPV_VERSION ${PC_MPV_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libmpv
    FOUND_VAR
        LIBMPV_FOUND
    REQUIRED_VARS
        LIBMPV_LIBRARIES
        LIBMPV_INCLUDE_DIRS
    VERSION_VAR
        LIBMPV_VERSION
)

if (LIBMPV_FOUND AND NOT TARGET Libmpv::Libmpv)
    add_library(Libmpv::Libmpv UNKNOWN IMPORTED)
    set_target_properties(Libmpv::Libmpv PROPERTIES
        IMPORTED_LOCATION "${LIBMPV_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBMPV_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(LIBMPV_LIBRARIES LIBMPV_INCLUDE_DIRS)

include(FeatureSummary)
set_package_properties(Libmpv PROPERTIES
    URL "https://mpv.io"
    DESCRIPTION "mpv media player client library"
)
