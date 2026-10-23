cmake_minimum_required(VERSION 3.19)

project(SdkPluginCheck LANGUAGES CXX)

find_package(Fooyin REQUIRED CONFIG)
qt_standard_project_setup()

create_fooyin_plugin(
    testbundleplugin
    JSON_IN testplugin.json.in
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Fooyin::Utils
    SOURCES testplugin.cpp
)
