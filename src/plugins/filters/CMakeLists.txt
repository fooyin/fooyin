cmake_minimum_required(VERSION 3.14)

project(
    fooyin_filters
    VERSION 0.3.0
    DESCRIPTION "Filters plugin for fooyin"
    HOMEPAGE_URL "https://github.com/ludouzi/fooyin"
    LANGUAGES CXX
)

create_fooyin_plugin(
    fooyin_filters
    SOURCES filtercontroller.cpp
            filtercolumnregistry.cpp
            filtersplugin.cpp
            filterdelegate.cpp
            filteritem.cpp
            filtermanager.cpp
            filtermodel.cpp
            filterpopulator.cpp
            filterview.cpp
            filterwidget.cpp
            settings/filterscolumnmodel.cpp
            settings/filtersettings.cpp
            settings/filterscolumnpage.cpp
            settings/filtersgeneralpage.cpp
)

target_link_libraries(
    fooyin_filters
    PRIVATE Fooyin::Gui
)
