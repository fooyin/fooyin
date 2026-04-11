function(windeployqt target)
    set(qt_bin_dir "$ENV{QT_ROOT_DIR}/bin")
    set(qt_host_bin_dir "$ENV{QT_HOST_PATH}/bin")

    set(windeployqt_hints "${qt_bin_dir}")
    if (DEFINED ENV{QT_HOST_PATH} AND NOT "$ENV{QT_HOST_PATH}" STREQUAL "")
        list(PREPEND windeployqt_hints "${qt_host_bin_dir}")
    endif ()

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
            --verbose 1
            --concurrent
            --no-translations
            --include-plugins qsqlite
    )
    set(WINDEPLOYQT_INSTALL_QTPATHS_ARG "")
    if (QTPATHS_EXECUTABLE)
        list(APPEND WINDEPLOYQT_COMMON_ARGS --qtpaths "${QTPATHS_EXECUTABLE}")
        string(APPEND WINDEPLOYQT_INSTALL_QTPATHS_ARG
                "\n                    --qtpaths \"${QTPATHS_EXECUTABLE}\""
        )
    endif ()

    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND $<IF:$<CONFIG:Debug>,${WINDEPLOYQT_DEBUG},${WINDEPLOYQT_RELEASE}>
            $<IF:$<CONFIG:Debug>,--debug,--release>
            ${WINDEPLOYQT_COMMON_ARGS}
                --dir "$<TARGET_FILE_DIR:${target}>"
                $<TARGET_FILE:${target}>
        COMMENT "Deploying Qt libraries for '${target}' ($<CONFIG>)..."
    )

    install(CODE "
        execute_process(
            COMMAND \"${WINDEPLOYQT_RELEASE}\"
                    --release
                    --verbose 1
                    --concurrent
                    --no-translations
                    --include-plugins qsqlite${WINDEPLOYQT_INSTALL_QTPATHS_ARG}
                    --dir \"\${CMAKE_INSTALL_PREFIX}/${BIN_INSTALL_DIR}\"
                    \"\${CMAKE_INSTALL_PREFIX}/${BIN_INSTALL_DIR}/${target}.exe\"
        )
    "
    COMPONENT fooyin
    )

    install(
        DIRECTORY "$<TARGET_FILE_DIR:fooyin>/"
        DESTINATION ${BIN_INSTALL_DIR}
        FILES_MATCHING PATTERN "*.dll"
    )
endfunction()
