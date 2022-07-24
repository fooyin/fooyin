set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(HEADERS
    src/plugin.h
    src/plugininfo.h
    src/pluginmanager.h
    src/pluginsystem_global.h
)

set(SOURCES
    src/plugininfo.cpp
    src/pluginmanager.cpp
)

add_library(PluginSystem SHARED ${SOURCES})

target_link_libraries(PluginSystem
    PRIVATE
    Qt6::Core
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
