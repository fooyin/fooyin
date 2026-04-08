function(fooyin_setup_macos_icon target)
    if(NOT APPLE)
        return()
    endif()

    set(_fooyin_macos_icns "${CMAKE_CURRENT_BINARY_DIR}/fooyin.icns")
    set(_fooyin_macos_iconset_dir "${CMAKE_CURRENT_BINARY_DIR}/fooyin.iconset")

    add_custom_command(
        OUTPUT "${_fooyin_macos_icns}"
        COMMAND ${CMAKE_COMMAND}
                -DICONSET_DIR="${_fooyin_macos_iconset_dir}"
                -DOUTPUT_ICNS="${_fooyin_macos_icns}"
                -DSOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/icons"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FooyinGenerateIcns.cmake"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/16-fooyin.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/32-fooyin.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/64-fooyin.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/128-fooyin.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/256-fooyin.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/512-fooyin.png"
    )

    add_custom_target(fooyin_macos_icon DEPENDS "${_fooyin_macos_icns}")
    add_dependencies(${target} fooyin_macos_icon)
    target_sources(${target} PRIVATE "${_fooyin_macos_icns}")
    set_source_files_properties(
        "${_fooyin_macos_icns}"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_target_properties(
        ${target}
        PROPERTIES MACOSX_BUNDLE_ICON_FILE "fooyin.icns"
    )
endfunction()
