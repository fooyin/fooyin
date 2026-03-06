function(fooyin_set_rpath target prefix)
    get_filename_component(path "${CMAKE_INSTALL_PREFIX}/${prefix}" ABSOLUTE)
    file(RELATIVE_PATH relative ${path} "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}")

    if(prefix STREQUAL "bin")
        if(APPLE)
            set(rpath "@executable_path/../lib/fooyin")
        else()
            set(rpath "\$ORIGIN/../lib/fooyin")
        endif()
    else()
        if(APPLE)
            set(rpath "@loader_path")
            if(prefix STREQUAL "lib/fooyin/plugins")
                set(rpath "@loader_path/..")
            endif()
        else()
            set(rpath "\$ORIGIN")
            if(prefix STREQUAL "lib/fooyin/plugins")
                set(rpath "\$ORIGIN/..")
            endif()
        endif()
    endif()

    if(CMAKE_INSTALL_RPATH)
        list(APPEND rpath ${CMAKE_INSTALL_RPATH})
    endif()

    set_target_properties(${target} PROPERTIES INSTALL_RPATH "${rpath}")

    if(APPLE AND NOT prefix STREQUAL "bin")
        set_target_properties(${target} PROPERTIES
            INSTALL_NAME_DIR "@rpath"
            BUILD_WITH_INSTALL_RPATH TRUE)
    endif()
endfunction()

function(create_fooyin_plugin plugin_name)
    cmake_parse_arguments(
        LIB
        "NO_INSTALL"
        "JSON_IN"
        "DEPENDS;SOURCES"
        ${ARGN}
    )

    set(CMAKE_AUTOMOC ON)

    string(TOLOWER ${plugin_name} name)
    set(output_name "fyplugin_${name}")

    # Configure json file:
    if (LIB_JSON_IN)
        set(json_file "${CMAKE_CURRENT_SOURCE_DIR}/${LIB_JSON_IN}")
    else()
        set(json_file "${CMAKE_CURRENT_SOURCE_DIR}/${name}.json.in")
    endif()

    if (EXISTS "${json_file}")
        list(APPEND LIB_SOURCES "${json_file}")
        configure_file("${json_file}" "${CMAKE_CURRENT_BINARY_DIR}/${name}.json")
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
                   OUTPUT_NAME ${output_name}
    )

    fooyin_set_rpath(${plugin_name} ${FOOYIN_PLUGIN_INSTALL_DIR})

    target_compile_definitions(${plugin_name} PRIVATE QT_USE_QSTRINGBUILDER)

    if(NOT CMAKE_SKIP_INSTALL_RULES AND NOT LIB_NO_INSTALL)
        install(
            TARGETS ${plugin_name}
            RUNTIME DESTINATION ${FOOYIN_PLUGIN_INSTALL_DIR}
            LIBRARY DESTINATION ${FOOYIN_PLUGIN_INSTALL_DIR}
        )
    endif()
endfunction()
