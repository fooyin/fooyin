include(GNUInstallDirs)

set(FOOYIN_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/fooyin")
set(FOOYIN_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}/fooyin")

set(FOOYIN_LIBRARY_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
set(FOOYIN_PLUGIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_PLUGIN_DIR})
set(FOOYIN_INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_INCLUDE_DIR})

set(FOOYIN_BINARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(FOOYIN_LIBRARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(FOOYIN_PLUGIN_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_PLUGIN_DIR})
set(FOOYIN_INCLUDE_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_INCLUDE_DIR})

if(${CMAKE_BUILD_TYPE} MATCHES Release)
    set(FOOYIN_PLUGIN_DIR ${FOOYIN_PLUGIN_INSTALL_DIR})
else()
    set(FOOYIN_PLUGIN_DIR ${FOOYIN_PLUGIN_OUTPUT_DIR})
endif()

function(fooyin_option option description)
    set(option_arguments ${ARGN})
    option(${option} "${description}" ${option_arguments})
    add_feature_info("Option ${option}" ${option} "${description}")
endfunction()

if(BUILD_SHARED_LIBS)
    set(FOOYIN_LIBRARY_TYPE SHARED)
    set(FOOYIN_PLUGIN_TYPE MODULE)
else()
    set(FOOYIN_LIBRARY_TYPE STATIC)
    set(FOOYIN_PLUGIN_TYPE STATIC)
endif()

function(create_fooyin_library name)
    cmake_parse_arguments(
        LIB
        ""
        "EXPORT_NAME"
        "SOURCES"
        ${ARGN}
    )

    add_library(${name} ${FOOYIN_LIBRARY_TYPE} ${LIB_SOURCES})
    add_library(Fooyin::${LIB_EXPORT_NAME} ALIAS ${name})

    string(TOLOWER ${LIB_EXPORT_NAME} base_name)

    include(CMakePackageConfigHelpers)
    include(GenerateExportHeader)
    generate_export_header(
        ${name}
        BASE_NAME
        fy${base_name}
        EXPORT_FILE_NAME
        export/fy${base_name}_export.h
    )

    if(NOT BUILD_SHARED_LIBS)
        string(TOUPPER ${LIB_EXPORT_NAME} static_name)
        target_compile_definitions(
            ${name} PUBLIC FY${static_name}_STATIC_DEFINE
        )
    endif()

    set(${base_name}_paths
        "$<INSTALL_PREFIX>/${FOOYIN_INCLUDE_DIR}/.."
        "$<INSTALL_PREFIX>/${FOOYIN_INCLUDE_DIR}"
        "$<INSTALL_PREFIX>/${FOOYIN_INCLUDE_DIR}/${base_name}"
    )

    target_include_directories(
        ${name}
        PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
               $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
               $<INSTALL_INTERFACE:${${base_name}_paths}>
    )

    target_include_directories(
        ${name} SYSTEM PUBLIC $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/export>
    )

    set_target_properties(
        ${name}
        PROPERTIES VERSION "${PROJECT_VERSION}"
                   CXX_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN YES
                   EXPORT_NAME ${LIB_EXPORT_NAME}
                   OUTPUT_NAME ${name}
                   LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_LIBRARY_OUTPUT_DIR}
                   INSTALL_RPATH ${FOOYIN_LIBRARY_INSTALL_DIR}
    )

    target_compile_features(${name} PUBLIC cxx_std_20)
    target_compile_definitions(${name} PRIVATE QT_USE_QSTRINGBUILDER)
    target_compile_options(
        ${name}
        PRIVATE -Werror
                -Wall
                -Wextra
                -Wpedantic
    )

    if(NOT CMAKE_SKIP_INSTALL_RULES)
        install(
            DIRECTORY "${PROJECT_BINARY_DIR}/export/"
            DESTINATION "${FOOYIN_INCLUDE_INSTALL_DIR}/${base_name}"
            COMPONENT fooyin_development
        )

        install(
            TARGETS ${name}
            EXPORT FooyinTargets
            LIBRARY DESTINATION ${FOOYIN_LIBRARY_DIR}
                    COMPONENT fooyin_development
        )
    endif()
endfunction()
