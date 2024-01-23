cmake_minimum_required(VERSION 3.14)

project(
    fooyin_tageditor
    VERSION 0.2.0
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

set_target_properties(
    fooyin_tageditor
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
               INSTALL_RPATH ${FOOYIN_PLUGIN_INSTALL_DIR}
)

if(NOT CMAKE_SKIP_INSTALL_RULES)
    install(TARGETS fooyin_tageditor DESTINATION ${FOOYIN_PLUGIN_INSTALL_DIR})
endif()
