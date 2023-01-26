include(GNUInstallDirs)

set(FOOYIN_BINARY_DIR ${CMAKE_INSTALL_BINDIR})
set(FOOYIN_LIBRARY_DIR "${CMAKE_INSTALL_LIBDIR}/fooyin")
set(FOOYIN_PLUGIN_DIR "${FOOYIN_LIBRARY_DIR}/plugins")

set(FOOYIN_HEADER_INSTALL_PATH "include/fooyin")
set(FOOYIN_LIBRARY_INSTALL_PATH ${CMAKE_INSTALL_PREFIX}/${FOOYIN_LIBRARY_DIR})
set(FOOYIN_PLUGIN_INSTALL_PATH ${CMAKE_INSTALL_PREFIX}/${FOOYIN_PLUGIN_DIR})

set(FOOYIN_BINARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_BINARY_DIR})
set(FOOYIN_LIBRARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_LIBRARY_DIR})
set(FOOYIN_PLUGIN_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_PLUGIN_DIR})

macro(fooyin_add_library name)
  set(CMAKE_AUTOUIC ON)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)

  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  cmake_parse_arguments(library
    ""
    "TYPE;PREFIX"
    "SOURCES;DEPENDS"
    ${ARGN}
  )

  set(library_name ${name})

  if(library_PREFIX)
    set(library_name "Fooyin_${library_name}")
  endif()

  add_library(${library_name} ${library_TYPE} ${library_SOURCES})
  target_link_libraries(${library_name} PRIVATE ${library_DEPENDS})

  cmake_path(SET library_build_interface "${CMAKE_CURRENT_SOURCE_DIR}/..")

  target_include_directories(${library_name}
    PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}"
      "${CMAKE_BINARY_DIR}/src"
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    PUBLIC
      "$<BUILD_INTERFACE:${library_build_interface}>"
   )

  target_compile_options(${library_name} PRIVATE -Werror -Wall -Wextra -Wpedantic)

  string(TOUPPER ${library_name} def_name)

  target_compile_definitions(${library_name} PRIVATE ${def_name}_LIBRARY)

  string(TOLOWER ${library_name} output_name)

  set_target_properties(${library_name} PROPERTIES
    LIBRARY_OUTPUT_NAME ${output_name}
    SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_LIBRARY_OUTPUT_DIR}
    INSTALL_RPATH ${FOOYIN_LIBRARY_INSTALL_PATH}
  )

  install(TARGETS ${library_name} LIBRARY DESTINATION ${FOOYIN_LIBRARY_DIR})
endmacro(fooyin_add_library)

macro(fooyin_add_plugin plugin_name)
  set(CMAKE_AUTOUIC ON)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)

  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  cmake_parse_arguments(plugin
    ""
    "TYPE"
    "SOURCES;DEPENDS"
    ${ARGN}
  )

  add_library(${plugin_name} ${plugin_TYPE} ${plugin_SOURCES})
  target_link_libraries(${plugin_name} PRIVATE ${plugin_DEPENDS})

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
    INSTALL_RPATH "${FOOYIN_LIBRARY_INSTALL_PATH};${FOOYIN_PLUGIN_INSTALL_PATH}"
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

  target_compile_options(${plugin_name} PRIVATE -Werror -Wall -Wextra -Wpedantic)

  install(TARGETS ${plugin_name} DESTINATION ${FOOYIN_PLUGIN_DIR})
endmacro(fooyin_add_plugin)
