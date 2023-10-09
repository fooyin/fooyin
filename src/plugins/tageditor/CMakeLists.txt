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
    fooyin_tageditor
    VERSION 0.1.0
    DESCRIPTION "Adds a widget to edit tags"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

create_fooyin_plugin(
    fooyin_tageditor
    SOURCES tageditoritem.cpp
            tageditorplugin.cpp
            tageditormodel.cpp
            tageditorwidget.cpp
)

target_link_libraries(
    fooyin_tageditor
    PRIVATE Fooyin::Gui
)
