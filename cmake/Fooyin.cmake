include(GNUInstallDirs)

set(FOOYIN_BINARY_DIR "${CMAKE_INSTALL_BINDIR}")
set(FOOYIN_LIBRARY_DIR "${CMAKE_INSTALL_LIBDIR}/fooyin")
set(FOOYIN_PLUGIN_DIR "${FOOYIN_LIBRARY_DIR}/plugins")

macro(fooyin_add_plugin plugin_name)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    set(CMAKE_INCLUDE_CURRENT_DIR ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    cmake_parse_arguments(plugin
      ""
      "TYPE"
      "SOURCES;PLUGIN_DEPENDS;DEPENDS"
      ${ARGN}
    )

   add_library(${plugin_name} ${plugin_TYPE} ${plugin_SOURCES})
   target_link_libraries(${plugin_name} PRIVATE ${plugin_PLUGIN_DEPENDS})
   target_link_libraries(${plugin_name} PRIVATE ${plugin_DEPENDS})

   set_target_properties(${plugin_name} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${FOOYIN_PLUGIN_DIR}")
   set_target_properties(${plugin_name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${FOOYIN_PLUGIN_DIR}")
   set_target_properties(${plugin_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FOOYIN_BINARY_DIR}")
   set_target_properties(${plugin_name} PROPERTIES PREFIX "")

   target_include_directories(${plugin_name} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

   install(TARGETS ${plugin_name} DESTINATION ${FOOYIN_PLUGIN_DIR})
endmacro(fooyin_add_plugin)

macro(fooyin_add_shared_library library_name)
    set_target_properties(${library_name} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${FOOYIN_LIBRARY_DIR}")
    set_target_properties(${library_name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${FOOYIN_LIBRARY_DIR}")
    set_target_properties(${library_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FOOYIN_BINARY_DIR}")
    set_target_properties(${library_name} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_BUILD_TARGET}
    )

    target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLUGINSYSTEM_LIBRARY)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${FOOYIN_LIBRARY_DIR})
endmacro(fooyin_add_shared_library)
