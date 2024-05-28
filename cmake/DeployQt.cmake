get_target_property(qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(qt_bin_dir "${qmake_executable}" DIRECTORY)

find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${qt_bin_dir}")
if (NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "windeployqt not found")
endif()

function(windeployqt target)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        find_program(WINDEPLOYQT_DEBUG windeployqt.debug.bat HINTS "${_qt_bin_dir}")
        if(WINDEPLOYQT_DEBUG)
            set(WINDEPLOYQT_EXECUTABLE = ${WINDEPLOYQT_DEBUG})
        endif()
        
        add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE}        
            --verbose 1
            --debug
            --include-plugins qsqlite
            \"$<TARGET_FILE:${target}>\"
        COMMENT "Deploying Qt libraries using windeployqt for compilation target '${target}' ..."
        )
    else()
        add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE}        
            --verbose 1
            --debug
            --include-plugins qsqlite
            \"$<TARGET_FILE:${target}>\"
        COMMENT "Deploying Qt libraries using windeployqt for compilation target '${target}' ..."
        )
    endif()

    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    foreach(lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
        get_filename_component(filename "${lib}" NAME)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E
                copy_if_different "${lib}" \"$<TARGET_FILE_DIR:${target}>\"
            COMMENT "Copying ${filename}"
        )
    endforeach()
endfunction()

mark_as_advanced(WINDEPLOYQT_EXECUTABLE)