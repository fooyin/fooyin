function(fooyin_set_rpath target prefix)
    get_filename_component(path "${CMAKE_INSTALL_PREFIX}/${prefix}" ABSOLUTE)
    file(RELATIVE_PATH relative ${path} "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}")
    set(rpath "\$ORIGIN/${relative}")
    if(CMAKE_INSTALL_RPATH)
        list(APPEND rpath ${CMAKE_INSTALL_RPATH})
    endif()
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "${rpath}")
endfunction()

function(create_fooyin_plugin base_name)
    cmake_parse_arguments(
        LIB
        ""
        ""
        "DEPENDS;SOURCES"
        ${ARGN}
    )

    set(CMAKE_AUTOMOC ON)

    string(TOLOWER ${base_name} name)
    set(plugin_name "fyplugin_${name}")

    # Configure json file:
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${name}.json.in")
      list(APPEND LIB_SOURCES ${name}.json.in)
      configure_file(${name}.json.in "${CMAKE_CURRENT_BINARY_DIR}/${name}.json")
    endif()

    add_library(${plugin_name} MODULE ${LIB_SOURCES})

    target_link_libraries(${plugin_name} PRIVATE ${LIB_DEPENDS})

    target_include_directories(
        ${plugin_name}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_CURRENT_BINARY_DIR}
    )

    if (NOT LIB_VERSION)
        set(LIB_VERSION ${FOOYIN_PLUGIN_VERSION})
    endif()

    set_target_properties(
        ${plugin_name}
        PROPERTIES VERSION ${LIB_VERSION}
                   CXX_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN YES
                   PREFIX ""
                   EXPORT_NAME ${name}
                   OUTPUT_NAME ${plugin_name}
    )

    fooyin_set_rpath(${plugin_name} ${FOOYIN_PLUGIN_INSTALL_DIR})

    target_compile_definitions(${plugin_name} PRIVATE QT_USE_QSTRINGBUILDER)

    if(NOT CMAKE_SKIP_INSTALL_RULES)
        install(TARGETS ${plugin_name} DESTINATION ${FOOYIN_PLUGIN_INSTALL_DIR} COMPONENT fooyin)
    endif()
endfunction()
