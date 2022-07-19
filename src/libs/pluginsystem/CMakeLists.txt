cmake_minimum_required(VERSION 3.14)

project(PluginSystem LANGUAGES CXX)

include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(FeatureSummary)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6Core)
set_package_properties(Qt6Core PROPERTIES TYPE REQUIRED)

set(HEADERS
    pluginsystem/plugin.h
    pluginsystem/plugininfo.h
    pluginsystem/pluginloader.h
    pluginsystem/pluginmanager.h
    pluginsystem/pluginsystem_global.h
)

set(SOURCES
    pluginsystem/plugininfo.cpp
    pluginsystem/pluginloader.cpp
    pluginsystem/pluginmanager.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Qt6::Core
)

target_compile_definitions(${PROJECT_NAME} PRIVATE PLUGINSYSTEM_LIBRARY)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
