function(windeployqt target)
    set(qt_bin_dir "$ENV{QT_ROOT_DIR}/bin")
    find_program(WINDEPLOYQT_RELEASE windeployqt HINTS "${qt_bin_dir}")
    find_program(WINDEPLOYQT_DEBUG windeployqt.debug.bat HINTS "${qt_bin_dir}")

    if(NOT WINDEPLOYQT_RELEASE)
        message(FATAL_ERROR "windeployqt not found in ${qt_bin_dir}")
    endif()

    if(NOT WINDEPLOYQT_DEBUG)
        set(WINDEPLOYQT_DEBUG ${WINDEPLOYQT_RELEASE})
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND $<IF:$<CONFIG:Debug>,${WINDEPLOYQT_DEBUG},${WINDEPLOYQT_RELEASE}>
                --verbose 1
                $<IF:$<CONFIG:Debug>,--debug,--release>
                --concurrent
                --no-translations
                --include-plugins qsqlite
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
                    --include-plugins qsqlite
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