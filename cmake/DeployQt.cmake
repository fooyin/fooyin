function(windeployqt target)
    set(qt_bin_dir "$ENV{QT_ROOT_DIR}/bin")
    set(qt_host_bin_dir "$ENV{QT_HOST_PATH}/bin")

    set(windeployqt_hints "${qt_bin_dir}")
    if(DEFINED ENV{QT_HOST_PATH} AND NOT "$ENV{QT_HOST_PATH}" STREQUAL "")
        list(PREPEND windeployqt_hints "${qt_host_bin_dir}")
    endif()

    find_program(WINDEPLOYQT_RELEASE windeployqt HINTS ${windeployqt_hints})
    find_program(WINDEPLOYQT_DEBUG windeployqt.debug.bat HINTS ${windeployqt_hints})
    find_program(QTPATHS_EXECUTABLE qtpaths qtpaths.bat HINTS "${qt_bin_dir}")

    if(NOT WINDEPLOYQT_RELEASE)
        message(FATAL_ERROR "windeployqt not found in ${windeployqt_hints}")
    endif()

    if(NOT WINDEPLOYQT_DEBUG)
        set(WINDEPLOYQT_DEBUG ${WINDEPLOYQT_RELEASE})
    endif()

    set(WINDEPLOYQT_COMMON_ARGS
        --verbose
        1
        --concurrent
        --no-translations
        --include-plugins
        qsqlite
    )
    set(WINDEPLOYQT_INSTALL_OPENGL_ARG "")
    if(TARGET Qt6::OpenGL)
        list(APPEND WINDEPLOYQT_COMMON_ARGS --opengl)
        string(APPEND WINDEPLOYQT_INSTALL_OPENGL_ARG "--opengl")
    endif()

    set(WINDEPLOYQT_INSTALL_QTPATHS_ARG "")
    if(QTPATHS_EXECUTABLE)
        list(APPEND WINDEPLOYQT_COMMON_ARGS --qtpaths "${QTPATHS_EXECUTABLE}")
        string(APPEND WINDEPLOYQT_INSTALL_QTPATHS_ARG "\n                    --qtpaths \"${QTPATHS_EXECUTABLE}\"")
    endif()

    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
            $<IF:$<CONFIG:Debug>,${WINDEPLOYQT_DEBUG},${WINDEPLOYQT_RELEASE}> $<IF:$<CONFIG:Debug>,--debug,--release>
            ${WINDEPLOYQT_COMMON_ARGS} --dir "$<TARGET_FILE_DIR:${target}>" $<TARGET_FILE:${target}>
        COMMENT "Deploying Qt libraries for '${target}' ($<CONFIG>)..."
    )

    install(
        CODE
            "
        execute_process(
            COMMAND \"${WINDEPLOYQT_RELEASE}\"
                    --release
                    --verbose 1
                    --concurrent
                    ${WINDEPLOYQT_INSTALL_OPENGL_ARG}
                    --no-translations
                    --include-plugins qsqlite${WINDEPLOYQT_INSTALL_QTPATHS_ARG}
                    --dir \"\${CMAKE_INSTALL_PREFIX}/${BIN_INSTALL_DIR}\"
                    \"\${CMAKE_INSTALL_PREFIX}/${BIN_INSTALL_DIR}/${target}.exe\"
        )
    "
        COMPONENT fooyin
    )

    # vcpkg deploys dependencies into the build tree. Skip plugin dependencies
    # that are identical to DLLs beside the exe.
    set(install_runtime_dlls
        [[
        set(runtime_dir "$<TARGET_FILE_DIR:fooyin>")
        file(GLOB_RECURSE runtime_dlls LIST_DIRECTORIES false RELATIVE "${runtime_dir}"
            "${runtime_dir}/*.[dD][lL][lL]")
        foreach(dll IN LISTS runtime_dlls)
            get_filename_component(dll_directory "${dll}" DIRECTORY)
            get_filename_component(dll_name "${dll}" NAME)
            if(dll_directory STREQUAL "@FOOYIN_RELATIVE_PLUGIN_PATH@" AND EXISTS "${runtime_dir}/${dll_name}")
                file(SHA256 "${runtime_dir}/${dll_name}" runtime_hash)
                file(SHA256 "${runtime_dir}/${dll}" plugin_hash)
                if(runtime_hash STREQUAL plugin_hash)
                    continue()
                endif()
            endif()
            file(INSTALL "${runtime_dir}/${dll}"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/@BIN_INSTALL_DIR@/${dll_directory}")
        endforeach()
    ]]
    )
    string(CONFIGURE "${install_runtime_dlls}" install_runtime_dlls @ONLY)
    install(CODE "${install_runtime_dlls}" COMPONENT fooyin)
endfunction()
