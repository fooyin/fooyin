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

    set_target_properties(
        ${name}
        PROPERTIES VERSION ${PROJECT_VERSION}
                   EXPORT_NAME ${base_name}
                   OUTPUT_NAME ${base_name}
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
endfunction()
