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

target_include_directories(
    fooyin_filters
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(
    fooyin_filters
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
               INSTALL_RPATH ${FOOYIN_PLUGIN_INSTALL_DIR}
)

if(NOT CMAKE_SKIP_INSTALL_RULES)
    install(TARGETS fooyin_filters DESTINATION ${FOOYIN_PLUGIN_INSTALL_DIR})
endif()
