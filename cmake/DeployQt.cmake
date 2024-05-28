get_target_property(qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(qt_bin_dir "${qmake_executable}" DIRECTORY)

function(windeployqt target)
    find_program(WINDEPLOYQT_RELEASE windeployqt HINTS "${qt_bin_dir}")
    find_program(WINDEPLOYQT_DEBUG   windeployqt.debug.bat HINTS "${qt_bin_dir}")

    if(NOT WINDEPLOYQT_RELEASE)
        message(FATAL_ERROR "windeployqt not found in ${qt_bin_dir}")
    endif()

    # Fallback
    if(NOT WINDEPLOYQT_DEBUG)
        set(WINDEPLOYQT_DEBUG ${WINDEPLOYQT_RELEASE})
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND $<$<CONFIG:Debug>:${WINDEPLOYQT_DEBUG}>
                $<$<NOT:$<CONFIG:Debug>>:${WINDEPLOYQT_RELEASE}>
            --verbose 1
            $<$<CONFIG:Debug>:--debug>
            $<$<NOT:$<CONFIG:Debug>>:--release>
            --include-plugins qsqlite
            --plugins platforms
            --dir "$<TARGET_FILE_DIR:${target}>"
            "$<TARGET_FILE:${target}>"
        COMMENT "Deploying Qt libraries using windeployqt for '${target}' ($<CONFIG>) ..."
    )

    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    foreach(lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
        get_filename_component(filename "${lib}" NAME)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E
                copy_if_different "${lib}" "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copying ${filename}"
        )
    endforeach()
endfunction()

mark_as_advanced(WINDEPLOYQT_RELEASE WINDEPLOYQT_DEBUG)
