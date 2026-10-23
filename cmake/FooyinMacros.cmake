function(fooyin_set_rpath target prefix)
    get_filename_component(path "${CMAKE_INSTALL_PREFIX}/${prefix}" ABSOLUTE)
    file(RELATIVE_PATH relative ${path} "${CMAKE_INSTALL_PREFIX}/${FOOYIN_LIBRARY_INSTALL_DIR}")
    if(APPLE)
        set(rpath "@loader_path/${relative}")
    else()
        set(rpath "\$ORIGIN/${relative}")
    endif()
    if(CMAKE_INSTALL_RPATH)
        list(APPEND rpath ${CMAKE_INSTALL_RPATH})
    endif()
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "${rpath}")
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
    if(LIB_JSON_IN)
        set(json_file "${CMAKE_CURRENT_SOURCE_DIR}/${LIB_JSON_IN}")
    else()
        set(json_file "${CMAKE_CURRENT_SOURCE_DIR}/${name}.json.in")
    endif()

    if(EXISTS "${json_file}")
        list(APPEND LIB_SOURCES "${json_file}")
        configure_file("${json_file}" "${CMAKE_CURRENT_BINARY_DIR}/${name}.json")
    endif()

    add_library(${plugin_name} MODULE ${LIB_SOURCES})

    target_link_libraries(${plugin_name} PRIVATE ${LIB_DEPENDS})

    target_include_directories(${plugin_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

    if(NOT LIB_VERSION)
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

function(create_fooyin_plugin_bundle plugin_target)
    cmake_parse_arguments(
        PARSE_ARGV
        1
        BUNDLE
        ""
        "NAME"
        "RUNTIME_TARGETS;RUNTIME_FILES;FILES;LICENSE_FILES;LICENSE_DIRECTORIES"
    )

    if(BUNDLE_UNPARSED_ARGUMENTS OR "NAME" IN_LIST BUNDLE_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid arguments to create_fooyin_plugin_bundle(${plugin_target})")
    endif()

    if(NOT BUNDLE_NAME)
        set(BUNDLE_NAME "${plugin_target}")
    endif()

    get_target_property(alias_target ${plugin_target} ALIASED_TARGET)
    if(alias_target)
        set(plugin_target "${alias_target}")
    endif()

    get_target_property(plugin_type ${plugin_target} TYPE)
    get_target_property(plugin_imported ${plugin_target} IMPORTED)
    if(NOT plugin_type STREQUAL "MODULE_LIBRARY" OR plugin_imported)
        message(FATAL_ERROR "create_fooyin_plugin_bundle requires a built plugin module")
    endif()

    if(WIN32)
        set(bundle_os windows)
    elseif(APPLE)
        set(bundle_os macos)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(bundle_os linux)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        set(bundle_os freebsd)
    else()
        message(FATAL_ERROR "Unsupported plugin bundle platform: ${CMAKE_SYSTEM_NAME}")
    endif()

    set(bundle_arch "${CMAKE_SYSTEM_PROCESSOR}")
    if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
        set(bundle_arch "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
    endif()
    string(TOLOWER "${bundle_arch}" bundle_arch)

    if(bundle_arch MATCHES "^(amd64|x86_64|x64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(bundle_arch x64)
    elseif(bundle_arch MATCHES "^(aarch64|arm64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(bundle_arch arm64)
    else()
        message(FATAL_ERROR "Unsupported plugin bundle architecture: ${bundle_arch}")
    endif()

    set(bundle_platform "${bundle_os}-${bundle_arch}")

    foreach(kind IN ITEMS RUNTIME_FILES FILES LICENSE_FILES LICENSE_DIRECTORIES)
        set(absolute_paths)
        foreach(path IN LISTS BUNDLE_${kind})
            if(NOT IS_ABSOLUTE "${path}" AND NOT path MATCHES "^\\$<")
                set(path "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
            endif()
            list(APPEND absolute_paths "${path}")
        endforeach()
        set(BUNDLE_${kind} "${absolute_paths}")
    endforeach()

    set(bundle_files "$<TARGET_FILE:${plugin_target}>")
    set(bundle_targets "${plugin_target}")

    foreach(target IN LISTS BUNDLE_RUNTIME_TARGETS)
        get_target_property(alias_target ${target} ALIASED_TARGET)
        if(alias_target)
            set(target "${alias_target}")
        endif()
        get_target_property(target_type ${target} TYPE)
        get_target_property(imported ${target} IMPORTED)

        if(NOT target_type STREQUAL "SHARED_LIBRARY" OR imported)
            message(
                FATAL_ERROR "RUNTIME_TARGETS requires built shared libraries; use RUNTIME_FILES for prebuilt libraries"
            )
        endif()

        get_target_property(no_soname ${target} NO_SONAME)

        if(WIN32 OR no_soname)
            list(APPEND bundle_files "$<TARGET_FILE:${target}>")
        else()
            list(APPEND bundle_files "$<TARGET_SONAME_FILE:${target}>")
        endif()

        list(APPEND bundle_targets "${target}")
    endforeach()

    list(APPEND bundle_files ${BUNDLE_RUNTIME_FILES})

    if(APPLE)
        set(bundle_rpath "@loader_path")
    elseif(UNIX)
        set(bundle_rpath "$ORIGIN")
    endif()

    if(UNIX)
        # Bundles copy build tree binaries directly
        foreach(target IN LISTS bundle_targets)
            set_target_properties(
                ${target}
                PROPERTIES BUILD_WITH_INSTALL_RPATH ON
                           INSTALL_RPATH "${bundle_rpath}"
                           INSTALL_RPATH_USE_LINK_PATH OFF
            )
        endforeach()
    endif()

    set(bundle_directory "${CMAKE_CURRENT_BINARY_DIR}/bundle")
    set(bundle_output "${CMAKE_CURRENT_BINARY_DIR}/${BUNDLE_NAME}-${bundle_platform}.fyplugin")

    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/FooyinPluginBundle.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/FooyinPluginBundle.cmake.in" @ONLY
    )
    file(
        GENERATE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/FooyinPluginBundle-$<CONFIG>.cmake"
        INPUT "${CMAKE_CURRENT_BINARY_DIR}/FooyinPluginBundle.cmake.in"
    )
    add_custom_target(
        package_plugin
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/FooyinPluginBundle-$<CONFIG>.cmake"
        DEPENDS ${bundle_targets} ${BUNDLE_RUNTIME_FILES} ${BUNDLE_FILES} ${BUNDLE_LICENSE_FILES}
        BYPRODUCTS "${bundle_output}"
        COMMENT "Packaging ${BUNDLE_NAME}-${bundle_platform}.fyplugin"
        VERBATIM
    )
endfunction()
