cmake_minimum_required(VERSION 3.14)

project(
    fooyin_utils
    VERSION 0.1.0
    DESCRIPTION "Utils library for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(SOURCES
    clickablelabel.cpp
    comboicon.cpp
    hovermenu.cpp
    id.cpp
    menuheader.cpp
    overlayfilter.cpp
    overlaywidget.cpp
    paths.cpp
    simplelistview.cpp
    scrollarea.cpp
    slider.cpp
    tablemodel.cpp
    utils.cpp
    worker.cpp

    actions/actioncontainer.cpp
    actions/actionmanager.cpp

    settings/settingsdialog.cpp
    settings/settingsdialogcontroller.cpp
    settings/settingspage.cpp
    settings/settingsmodel.cpp
    settings/settingsmanager.h
    settings/settingsmanager.cpp
    settings/settingsentry.cpp
    settings/settingtypes.h
)

add_library(
    fooyin_utils
    ${FOOYIN_LIBRARY_TYPE}
    ${SOURCES}
)
add_library(fooyin::utils ALIAS fooyin_utils)

target_include_directories(fooyin_utils
  PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/src"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
)

set_target_properties(fooyin_utils PROPERTIES
    VERSION "${PROJECT_VERSION}"
    EXPORT_NAME fooyin_utils
    OUTPUT_NAME fooyin_utils
    LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_LIBRARY_OUTPUT_DIR}
    INSTALL_RPATH ${FOOYIN_LIBRARY_INSTALL_PATH}
)

target_link_libraries(fooyin_utils
   PRIVATE
     Qt6::Core
     Qt6::Widgets
)

target_compile_features(fooyin_utils PUBLIC cxx_std_17)
target_compile_options(fooyin_utils PRIVATE -Werror -Wall -Wextra -Wpedantic)

install(TARGETS fooyin_utils LIBRARY DESTINATION ${FOOYIN_LIBRARY_DIR})
