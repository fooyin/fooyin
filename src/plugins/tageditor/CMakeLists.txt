cmake_minimum_required(VERSION 3.14)

project(
    fooyin_tageditor
    VERSION 0.3.0
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
