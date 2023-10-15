#
# Fooyin
# Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
#
# Fooyin is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Fooyin is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
#

cmake_minimum_required(VERSION 3.14)

project(
    fooyin_utils
    VERSION 0.1.0
    DESCRIPTION "fooyin_utils library for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(HEADERS
    ${CMAKE_SOURCE_DIR}/include/utils/clickablelabel.h
    ${CMAKE_SOURCE_DIR}/include/utils/comboicon.h
    ${CMAKE_SOURCE_DIR}/include/utils/expandingcombobox.h
    ${CMAKE_SOURCE_DIR}/include/utils/extentabletableview.h
    ${CMAKE_SOURCE_DIR}/include/utils/headerview.h
    ${CMAKE_SOURCE_DIR}/include/utils/hovermenu.h
    ${CMAKE_SOURCE_DIR}/include/utils/itemregistry.h
    ${CMAKE_SOURCE_DIR}/include/utils/menuheader.h
    ${CMAKE_SOURCE_DIR}/include/utils/multilinedelegate.h
    ${CMAKE_SOURCE_DIR}/include/utils/overlayfilter.h
    ${CMAKE_SOURCE_DIR}/include/utils/slider.h
    ${CMAKE_SOURCE_DIR}/include/utils/worker.h
    ${CMAKE_SOURCE_DIR}/include/utils/actions/actioncontainer.h
    ${CMAKE_SOURCE_DIR}/include/utils/actions/actionmanager.h
    ${CMAKE_SOURCE_DIR}/include/utils/settings/settingsdialogcontroller.h
    ${CMAKE_SOURCE_DIR}/include/utils/settings/settingsentry.h
    ${CMAKE_SOURCE_DIR}/include/utils/settings/settingsmanager.h
    ${CMAKE_SOURCE_DIR}/include/utils/settings/settingspage.h
)

set(SOURCES
    clickablelabel.cpp
    comboicon.cpp
    crypto.cpp
    expandingcombobox.cpp
    extentabletableview.cpp
    headerview.cpp
    hovermenu.cpp
    id.cpp
    menuheader.cpp
    multilinedelegate.cpp
    overlayfilter.cpp
    overlaywidget.cpp
    paths.cpp
    simpletreeview.cpp
    scrollarea.cpp
    slider.cpp
    utils.cpp
    worker.cpp
    actions/actioncontainer.cpp
    actions/actionmanager.cpp
    settings/settingsdialog.cpp
    settings/settingsdialogcontroller.cpp
    settings/settingspage.cpp
    settings/settingsmodel.cpp
    settings/settingsmanager.cpp
    settings/settingsentry.cpp
)

create_fooyin_library(
    fooyin_utils
    EXPORT_NAME Utils
    SOURCES ${HEADERS} ${SOURCES}
)

target_link_libraries(
    fooyin_utils
    PUBLIC Qt6::Core
           Qt6::Widgets
           Qt6::Sql
           Qt6::Concurrent
)
