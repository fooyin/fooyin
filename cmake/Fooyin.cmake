set(FOOYIN_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

set(FOOYIN_SOURCE_DIR "${FOOYIN_ROOT_DIR}/src")
set(FOOYIN_PLUGINS_SOURCE_DIR "${FOOYIN_SOURCE_DIR}/plugins")

set(FOOYIN_BINARY_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(FOOYIN_LIBRARY_DIR "${FOOYIN_BINARY_DIR}/../libs/fooyin")
set(FOOYIN_PLUGIN_DIR "${FOOYIN_LIBRARY_DIR}/plugins")

macro(fooyin_add_sources)
    set(plugin_sources "")

    foreach(source IN ITEMS ${ARGN})
        list(APPEND plugin_sources "${source}")
    endforeach()

    if (NOT target_${plugin_name})
        set(target_${plugin_name} TRUE)
        add_library(${plugin_name} SHARED ${plugin_sources})
    else()
        target_sources(${plugin_name} PRIVATE "${plugin_sources}")
    endif()
endmacro(fooyin_add_sources)

macro(fooyin_use_qt_libraries)
    set(plugin_find_list "")
    set(plugin_link_list "")

    foreach(lib IN ITEMS ${ARGN})
        list(APPEND plugin_find_list "${lib}")
        list(APPEND plugin_link_list "Qt6::${lib}")
    endforeach()

    find_package(Qt6 COMPONENTS ${plugin_find_list} REQUIRED)
    target_link_libraries(${plugin_name} PRIVATE ${plugin_link_list})
endmacro(fooyin_use_qt_libraries)

macro(fooyin_use_plugins)
    set(plugin_link_list "")

    foreach(plugin IN ITEMS ${ARGN})
        list(APPEND plugin_link_list "${plugin}")
    endforeach()

    target_link_libraries(${plugin_name} PRIVATE ${plugin_link_list})
endmacro(fooyin_use_plugins)

macro(fooyin_use_shared_libraries)
    set(plugin_link_list "")

    foreach(arg IN ITEMS ${ARGN})
        list(APPEND plugin_link_list "${arg}")
    endforeach()

    target_link_libraries(${plugin_name} PRIVATE ${plugin_link_list})
endmacro(fooyin_use_shared_libraries)

macro(fooyin_end_shared_library)
    set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${FOOYIN_LIBRARY_DIR}")
    set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${FOOYIN_LIBRARY_DIR}")
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FOOYIN_LIBRARY_DIR}")

    target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLUGINSYSTEM_LIBRARY)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${FOOYIN_LIBRARY_DIR})
endmacro(fooyin_end_shared_library)

macro(fooyin_start_plugin)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    set(CMAKE_INCLUDE_CURRENT_DIR ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    set(plugin_name ${PROJECT_NAME})
endmacro(fooyin_start_plugin)

macro(fooyin_end_plugin)
    set_target_properties(${plugin_name} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${FOOYIN_PLUGIN_DIR}")
    set_target_properties(${plugin_name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${FOOYIN_PLUGIN_DIR}")
    set_target_properties(${plugin_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FOOYIN_PLUGIN_DIR}")
    target_include_directories(${plugin_name} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

    install(TARGETS ${plugin_name} DESTINATION ${FOOYIN_PLUGIN_DIR})
endmacro(fooyin_end_plugin)
