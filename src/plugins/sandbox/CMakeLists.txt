cmake_minimum_required(VERSION 3.14)

project(
    fooyin_sandbox
    VERSION 0.1.0
    DESCRIPTION "Provides a sandbox for testing scripts"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(SOURCES
    sandboxdialog.cpp
    sandboxplugin.cpp
    scripthighlighter.cpp
    expressiontreemodel.cpp
)

#qt_add_resources(
#    SOURCES
#    data/icons.qrc
#)

add_library(fooyin_sandbox SHARED ${SOURCES})
target_link_libraries(fooyin_sandbox
    PRIVATE
      Qt6::Core
      Qt6::Widgets
      Qt6::Sql
      fooyin::core
      fooyin::gui
      fooyin::utils
)

set_target_properties(fooyin_sandbox PROPERTIES
  VERSION "${PROJECT_VERSION}"
  EXPORT_NAME fooyin_sandbox
  OUTPUT_NAME fooyin_sandbox
  LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
  INSTALL_RPATH "${FOOYIN_PLUGIN_INSTALL_PATH}"
)

target_include_directories(fooyin_sandbox
  PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/src"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
)

target_compile_options(fooyin_sandbox PRIVATE -Werror -Wall -Wextra -Wpedantic)

install(TARGETS fooyin_sandbox DESTINATION ${FOOYIN_PLUGIN_DIR})
