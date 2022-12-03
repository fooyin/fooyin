include(GNUInstallDirs)

set(FOOYIN_BINARY_DIR "${CMAKE_INSTALL_BINDIR}")
set(FOOYIN_LIBRARY_DIR "${CMAKE_INSTALL_LIBDIR}/fooyin")
set(FOOYIN_PLUGIN_DIR "${FOOYIN_LIBRARY_DIR}/plugins")

set(FOOYIN_HEADER_INSTALL_PATH "include/fooyin")

set(FOOYIN_BINARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_BINARY_DIR})
set(FOOYIN_LIBRARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_LIBRARY_DIR})
set(FOOYIN_PLUGIN_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_PLUGIN_DIR})

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
  target_link_libraries(${plugin_name} PRIVATE ${plugin_DEPENDS} PUBLIC ${plugin_PLUGIN_DEPENDS})

  string(TOLOWER ${plugin_name} output_name)

  set_target_properties(${plugin_name} PROPERTIES
    LINK_DEPENDS_NO_SHARED ON
    SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${FOOYIN_PLUGIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${FOOYIN_PLUGIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${FOOYIN_BINARY_DIR}"
    PREFIX "fooyin_"
    OUTPUT_NAME ${output_name}
    LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${FOOYIN_PLUGIN_DIR}"
  )

  cmake_path(SET public_build_interface "${CMAKE_CURRENT_SOURCE_DIR}/..")

  target_include_directories(${plugin_name}
    PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}"
      "${CMAKE_BINARY_DIR}/src"
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    PUBLIC
      "$<BUILD_INTERFACE:${public_build_interface}>"
   )

  install(TARGETS ${plugin_name} DESTINATION ${FOOYIN_PLUGIN_DIR})
endmacro(fooyin_add_plugin)
