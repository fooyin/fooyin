#
# Fooyin Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
#
# Fooyin is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# Fooyin is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# Fooyin.  If not, see <http://www.gnu.org/licenses/>.
#

cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

include(CheckBuildDir)

project(
    fooyin
    VERSION 0.1.0
    DESCRIPTION "A customisable music player"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

include(CheckLevel)
include(Fooyin)
include(FooyinMacros)

include(GenerateExportHeader)
include(FeatureSummary)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE STRING "" FORCE
    )
endif()

fooyin_option(BUILD_SHARED_LIBS "Build fooyin libraries as shared" ON)
fooyin_option(BUILD_TESTING "Build fooyin tests" OFF)

find_package(
    Qt6 REQUIRED
    COMPONENTS Core
               Widgets
               Gui
               Sql
               Concurrent
)
set_package_properties(Qt6Core PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Widgets PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Gui PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Sql PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6Concurrent PROPERTIES TYPE REQUIRED)

find_package(QCoro6 REQUIRED COMPONENTS Core)
qcoro_enable_coroutines()

find_package(Taglib REQUIRED taglib>=1.12)
find_package(Libmpv REQUIRED)

# find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL AVDEVICE AVFILTER
# SWSCALE SWRESAMPLE )

# ---- Fooyin library ----

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

configure_file(src/app/version.h.in app/version.h @ONLY)
add_library(fooyin_version OBJECT)
target_include_directories(fooyin_version PUBLIC "${PROJECT_BINARY_DIR}/app")
configure_file(src/app/config.h.in app/config.h @ONLY)
add_library(fooyin_config OBJECT)
target_include_directories(fooyin_config PUBLIC "${PROJECT_BINARY_DIR}/app")

add_library(fooyin_lib OBJECT src/app/singleinstance.cpp)

target_include_directories(
    fooyin_lib PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
                      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/app>"
)

set_target_properties(
    fooyin_lib PROPERTIES VERSION "${PROJECT_VERSION}" EXPORT_NAME Fooyin
)

add_subdirectory(src)

target_link_libraries(fooyin_lib PUBLIC Fooyin::Core Fooyin::Gui Fooyin::Utils)

target_compile_features(fooyin_lib PUBLIC cxx_std_20)
target_compile_options(
    fooyin_lib
    PUBLIC -Werror
           -Wall
           -Wextra
           -Wpedantic
)

# ---- Fooyin testing ----

if(BUILD_TESTING)
    find_package(GTest)
    add_subdirectory(tests)
endif()

# ---- Fooyin executable ----

set(SOURCES src/app/main.cpp)

qt_add_resources(SOURCES data/icons.qrc)

add_executable(fooyin_exe ${SOURCES})

set_target_properties(
    fooyin_exe
    PROPERTIES OUTPUT_NAME fooyin
               RUNTIME_OUTPUT_DIRECTORY ${FOOYIN_BINARY_OUTPUT_DIR}
               INSTALL_RPATH ${FOOYIN_LIBRARY_INSTALL_DIR}
)
target_compile_features(fooyin_exe PUBLIC cxx_std_20)
target_compile_options(
    fooyin_exe
    PRIVATE -Werror
            -Wall
            -Wextra
            -Wpedantic
)

target_link_libraries(fooyin_exe PRIVATE fooyin_version fooyin_lib)

# ---- Fooyin data ----

set(SHARE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/share)
install(FILES ${CMAKE_SOURCE_DIR}/dist/fooyin.desktop
        DESTINATION ${SHARE_INSTALL_PREFIX}/applications
)
set(ICON_SRC_PATH ${CMAKE_SOURCE_DIR}/data/icons)
set(ICON_SIZE
    16
    22
    32
    48
    64
    128
    256
    512
)
foreach(SIZE ${ICON_SIZE})
    install(
        FILES ${ICON_SRC_PATH}/${SIZE}-fooyin.png
        DESTINATION ${SHARE_INSTALL_PREFIX}/icons/hicolor/${SIZE}x${SIZE}/apps
        RENAME fooyin.png
    )
endforeach(SIZE)

install(
    FILES ${ICON_SRC_PATH}/sc-fooyin.svg
    DESTINATION ${SHARE_INSTALL_PREFIX}/icons/hicolor/scalable/apps
    RENAME fooyin.svg
)

# ---- Fooyin install ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(FooyinInstall)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
