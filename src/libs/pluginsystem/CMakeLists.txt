set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(HEADERS
    plugin.h
    plugininfo.h
    pluginmanager.h
    pluginsystem_global.h
)

set(SOURCES
    plugininfo.cpp
    pluginmanager.cpp
)

add_library(PluginSystem SHARED ${SOURCES})

target_link_libraries(PluginSystem
    PRIVATE
    Qt6::Core
)

cmake_path(SET public_build_interface "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_include_directories(PluginSystem
  PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/src"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  PUBLIC
    "$<BUILD_INTERFACE:${public_build_interface}>"
 )

target_include_directories(PluginSystem INTERFACE include)

target_compile_definitions(PluginSystem PRIVATE PLUGINSYSTEM_LIBRARY)

set_target_properties(PluginSystem PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${FOOYIN_LIBRARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${FOOYIN_LIBRARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${FOOYIN_BINARY_DIR}"
    LIBRARY_OUTPUT_NAME "plugin_system"
    LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_LIBRARY_OUTPUT_DIR}
)

install(TARGETS PluginSystem LIBRARY DESTINATION ${FOOYIN_LIBRARY_DIR})
