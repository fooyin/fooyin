cmake_minimum_required(VERSION 3.14)

project(
    fooyin_filters
    VERSION 0.1.0
    DESCRIPTION "Filters plugin for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(SOURCES
    constants.h
    fieldparser.h
    fieldparser.cpp
    fieldregistry.h
    fieldregistry.cpp
    filtersplugin.h
    filtersplugin.cpp
    filterdelegate.h
    filterdelegate.cpp
    fielditem.h
    fielditem.cpp
    fieldmodel.h
    fieldmodel.cpp
    filtersappearancepage.h
    filtersappearancepage.cpp
    filtersfieldspage.h
    filtersfieldspage.cpp
    filtersgeneralpage.h
    filtersgeneralpage.cpp
    filteritem.h
    filteritem.cpp
    filtermanager.h
    filtermanager.cpp
    filtermodel.h
    filtermodel.cpp
    filtersettings.h
    filtersettings.cpp
    filterview.h
    filterview.cpp
    filterwidget.h
    filterwidget.cpp
    filterfwd.h
    filterstore.h
    filterstore.cpp
    searchwidget.h
    searchwidget.cpp
    trackfilterer.h
    trackfilterer.cpp
)

qt_add_resources(
    SOURCES
    data/icons.qrc
)

add_library(fooyin_filters SHARED ${SOURCES})
target_link_libraries(fooyin_filters
    PRIVATE
      Qt6::Core
      Qt6::Widgets
      Qt6::Sql
      fooyin::core
      fooyin::gui
      fooyin::utils
)

set_target_properties(fooyin_filters PROPERTIES
  VERSION "${PROJECT_VERSION}"
  EXPORT_NAME fooyin_filters
  OUTPUT_NAME fooyin_filters
  LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
  INSTALL_RPATH "${FOOYIN_PLUGIN_INSTALL_PATH}"
)

target_include_directories(fooyin_filters
  PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/src"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
)

target_compile_options(fooyin_filters PRIVATE -Werror -Wall -Wextra -Wpedantic)

install(TARGETS fooyin_filters DESTINATION ${FOOYIN_PLUGIN_DIR})
