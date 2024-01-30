function(create_fooyin_plugin name)
    cmake_parse_arguments(
        LIB
        ""
        ""
        "SOURCES"
        ${ARGN}
    )

    set(CMAKE_AUTOMOC ON)

    add_library(${name} MODULE ${LIB_SOURCES})

    string(TOLOWER ${name} base_name)

    target_include_directories(
        ${name}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_target_properties(
        ${name}
        PROPERTIES VERSION ${PROJECT_VERSION}
                   CXX_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN YES
                   EXPORT_NAME ${base_name}
                   OUTPUT_NAME ${base_name}
                   BUILD_RPATH "${FOOYIN_PLUGIN_RPATH};${CMAKE_BUILD_RPATH}"
                   INSTALL_RPATH "${FOOYIN_PLUGIN_RPATH};${CMAKE_INSTALL_RPATH}"
                   RUNTIME_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
                   LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
                   ARCHIVE_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
    )

    target_compile_features(${name} PUBLIC ${FOOYIN_REQUIRED_CXX_FEATURES})
    target_compile_definitions(${name} PRIVATE QT_USE_QSTRINGBUILDER)
    target_compile_options(
        ${name}
        PRIVATE -Werror
                -Wall
                -Wextra
                -Wpedantic
    )

    if(NOT CMAKE_SKIP_INSTALL_RULES)
        install(TARGETS ${name} DESTINATION ${FOOYIN_PLUGIN_INSTALL_DIR})
    endif()
endfunction()
