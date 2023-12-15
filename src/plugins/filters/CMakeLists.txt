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
    fooyin_filters
    VERSION 0.1.0
    DESCRIPTION "Filters plugin for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

create_fooyin_plugin(
    fooyin_filters
    SOURCES filtercontroller.cpp
            filtercolumnregistry.cpp
            filtersplugin.cpp
            filterdelegate.cpp
            filteritem.cpp
            filtermanager.cpp
            filtermodel.cpp
            filterpopulator.cpp
            filterview.cpp
            filterwidget.cpp
            settings/filterscolumnmodel.cpp
            settings/filtersettings.cpp
            settings/filterscolumnpage.cpp
            settings/filtersgeneralpage.cpp
)

target_link_libraries(
    fooyin_filters
    PRIVATE Fooyin::Gui
)

target_include_directories(
    fooyin_filters
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(
    fooyin_filters
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
               INSTALL_RPATH ${FOOYIN_PLUGIN_INSTALL_DIR}
)

if(NOT CMAKE_SKIP_INSTALL_RULES)
    install(TARGETS fooyin_filters DESTINATION ${FOOYIN_PLUGIN_INSTALL_DIR})
endif()
