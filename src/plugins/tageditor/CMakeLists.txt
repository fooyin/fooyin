cmake_minimum_required(VERSION 3.14)

project(
    fooyin_tageditor
    VERSION 0.1.0
    DESCRIPTION "Adds a widget to edit tags"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

set(SOURCES tageditoritem.cpp tageditorplugin.cpp tageditormodel.cpp tageditorwidget.cpp)

add_library(fooyin_tageditor SHARED ${SOURCES})
target_link_libraries(
    fooyin_tageditor
    PRIVATE Qt6::Core
            Qt6::Widgets
            fooyin::core
            fooyin::gui
            fooyin::utils
)

set_target_properties(
    fooyin_tageditor
    PROPERTIES VERSION "${PROJECT_VERSION}"
               EXPORT_NAME fooyin_tageditor
               OUTPUT_NAME fooyin_tageditor
               LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
               INSTALL_RPATH "${FOOYIN_PLUGIN_INSTALL_PATH}"
)

target_include_directories(
    fooyin_tageditor
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            "${CMAKE_BINARY_DIR}/src"
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
)

target_compile_options(fooyin_tageditor PRIVATE -Werror -Wall -Wextra -Wpedantic)

install(TARGETS fooyin_tageditor DESTINATION ${FOOYIN_PLUGIN_DIR})
