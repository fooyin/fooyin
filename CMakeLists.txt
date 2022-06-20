cmake_minimum_required(VERSION 3.16)
project(fooyin LANGUAGES CXX C)

set(FOOYIN_VERSION "0.1.0")
set(FOOYIN_DATABASE_VERSION "0.8.0")

include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(FeatureSummary)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(AUTOMOC_PATH_PREFIX ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

configure_file(version.h.in "${CMAKE_CURRENT_BINARY_DIR}/src/utils/version.h" @ONLY)

find_package(Qt6Core)
set_package_properties(Qt6Core PROPERTIES TYPE REQUIRED)

find_package(Qt6Widgets)
set_package_properties(Qt6Widgets PROPERTIES TYPE REQUIRED)

find_package(Qt6Gui)
set_package_properties(Qt6Gui PROPERTIES TYPE REQUIRED)

find_package(Qt6Sql)
set_package_properties(Qt6Sql PROPERTIES TYPE REQUIRED)

find_package(Taglib REQUIRED taglib>=1.12)
set_package_properties(Taglib PROPERTIES TYPE REQUIRED)

find_package(Libmpv)
set_package_properties(Libmpv PROPERTIES TYPE REQUIRED)

#find_package(FFmpeg
#                COMPONENTS
#                AVCODEC
#                AVFORMAT
#                AVUTIL
#                AVDEVICE
#                AVFILTER
#                SWSCALE
#                SWRESAMPLE
#)
#set_package_properties(FFmpeg PROPERTIES TYPE REQUIRED)

add_subdirectory(src)

set(SHARE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/share)

install(FILES ${CMAKE_SOURCE_DIR}/dist/fooyin.desktop DESTINATION ${SHARE_INSTALL_PREFIX}/applications)

set(ICON_SRC_PATH ${CMAKE_SOURCE_DIR}/data/icons)

set(ICON_SIZE 16 22 32 48 64 128 256 512)
foreach(SIZE ${ICON_SIZE})
    install(FILES ${ICON_SRC_PATH}/${SIZE}-fooyin.png DESTINATION
                       ${SHARE_INSTALL_PREFIX}/icons/hicolor/${SIZE}x${SIZE}/apps RENAME fooyin.png)
endforeach(SIZE)
install(FILES ${ICON_SRC_PATH}/sc-fooyin.svg DESTINATION
                       ${SHARE_INSTALL_PREFIX}/icons/hicolor/scalable/apps RENAME fooyin.svg)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
