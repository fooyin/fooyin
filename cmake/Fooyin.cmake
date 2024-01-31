include(GNUInstallDirs)

set(FOOYIN_BIN_DIR ${CMAKE_INSTALL_BINDIR})
set(FOOYIN_LIBRARY_DIR "${CMAKE_INSTALL_LIBDIR}/fooyin")
set(FOOYIN_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/fooyin/plugins")
set(FOOYIN_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}/fooyin")

set(FOOYIN_BIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_BIN_DIR})
set(FOOYIN_LIBRARY_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_LIBRARY_DIR})
set(FOOYIN_PLUGIN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_PLUGIN_DIR})
set(FOOYIN_INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${FOOYIN_INCLUDE_DIR})

set(FOOYIN_BINARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_BIN_DIR})
set(FOOYIN_LIBRARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_LIBRARY_DIR})
set(FOOYIN_PLUGIN_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_PLUGIN_DIR})
set(FOOYIN_INCLUDE_OUTPUT_DIR ${PROJECT_BINARY_DIR}/${FOOYIN_INCLUDE_DIR})

file(RELATIVE_PATH FOOYIN_RELATIVE_LIB_PATH "/${FOOYIN_BIN_DIR}" "/${FOOYIN_LIBRARY_DIR}")
file(RELATIVE_PATH FOOYIN_PLUGIN_TO_LIB "/${FOOYIN_PLUGIN_DIR}" "/${FOOYIN_LIBRARY_DIR}")
file(RELATIVE_PATH FOOYIN_RELATIVE_PLUGIN_PATH "/${CMAKE_INSTALL_BINDIR}" "/${FOOYIN_PLUGIN_DIR}")

set(FOOYIN_RPATH "\$ORIGIN")
set(FOOYIN_LIBRARY_RPATH "\$ORIGIN")
set(FOOYIN_PLUGIN_RPATH "\$ORIGIN;\$ORIGIN/${FOOYIN_PLUGIN_TO_LIB}")

function(fooyin_option option description)
    set(option_arguments ${ARGN})
    option(${option} "${description}" ${option_arguments})
    add_feature_info("Option ${option}" ${option} "${description}")
endfunction()

function(create_fooyin_library name)
    cmake_parse_arguments(
        LIB
        ""
        "EXPORT_NAME"
        "SOURCES;PRIVATE_SOURCES"
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
        set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
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
        ${name} SYSTEM PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/export>
    )

    set_target_properties(
        ${name}
        PROPERTIES VERSION ${PROJECT_VERSION}
                   SOVERSION "${PROJECT_VERSION_MAJOR}.0.0"
                   CXX_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN YES
                   EXPORT_NAME ${LIB_EXPORT_NAME}
                   OUTPUT_NAME ${name}
                   BUILD_RPATH "${FOOYIN_LIBRARY_RPATH};${CMAKE_BUILD_RPATH}"
                   INSTALL_RPATH "${FOOYIN_LIBRARY_RPATH};${CMAKE_INSTALL_RPATH}"
                   RUNTIME_OUTPUT_DIRECTORY ${FOOYIN_LIBRARY_OUTPUT_DIR}
                   LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_LIBRARY_OUTPUT_DIR}
                   ARCHIVE_OUTPUT_DIRECTORY ${FOOYIN_LIBRARY_OUTPUT_DIR}
    )

    target_compile_features(${name} PUBLIC ${FOOYIN_REQUIRED_CXX_FEATURES})
    target_compile_definitions(${name} PRIVATE QT_USE_QSTRINGBUILDER)
    target_compile_options(${name} PRIVATE ${FOOYIN_COMPILE_OPTIONS})

    if(LIB_PRIVATE_SOURCES)
        set(private_name "${name}_private")

        add_library(${private_name} STATIC ${PRIVATE_SOURCES})
        add_library(Fooyin::${LIB_EXPORT_NAME}Private ALIAS ${private_name})

        target_include_directories(
            ${private_name}
            PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        )

        set_target_properties(
            ${private_name}
            PROPERTIES CXX_VISIBILITY_PRESET hidden
                       VISIBILITY_INLINES_HIDDEN YES
                       POSITION_INDEPENDENT_CODE ON
        )
        target_compile_features(${private_name} PUBLIC ${FOOYIN_REQUIRED_CXX_FEATURES})
        target_compile_definitions(${private_name} PRIVATE QT_USE_QSTRINGBUILDER)
        target_compile_options(${name} PRIVATE ${FOOYIN_COMPILE_OPTIONS})

        target_link_libraries(${private_name} PRIVATE ${name})

        if(BUILD_PCH)
            target_precompile_headers(${private_name} REUSE_FROM fooyin_pch)
        endif()
    endif()

    if(BUILD_PCH)
        target_precompile_headers(${name} REUSE_FROM fooyin_pch)
    endif()

    if(NOT CMAKE_SKIP_INSTALL_RULES)
        install(
            DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/export/"
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
