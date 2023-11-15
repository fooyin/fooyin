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

function(create_fooyin_plugin name)
    cmake_parse_arguments(
        LIB
        ""
        ""
        "SOURCES"
        ${ARGN}
    )

    add_library(${name} MODULE ${LIB_SOURCES})

    string(TOLOWER ${name} base_name)

    set_target_properties(
        ${name}
        PROPERTIES VERSION ${PROJECT_VERSION}
                   EXPORT_NAME ${base_name}
                   OUTPUT_NAME ${base_name}
                   LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
                   INSTALL_RPATH ${FOOYIN_PLUGIN_INSTALL_DIR}
    )

    target_compile_features(${name} PUBLIC ${FOOYIN_REQUIRED_CXX_FEATURES})
    target_compile_definitions(${name} PRIVATE QT_USE_QSTRINGBUILDER)
    target_compile_options(
        ${name}
        PRIVATE -Werror
                -Wall
                -Wextra
                -Wpedantic
    )

    if(NOT CMAKE_SKIP_INSTALL_RULES)
        install(TARGETS ${name} DESTINATION ${FOOYIN_PLUGIN_INSTALL_DIR})
    endif()
endfunction()
