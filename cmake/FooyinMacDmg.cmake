function(fooyin_prepare_macos_dmg_assets)
    if(NOT APPLE)
        return()
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/macos/fooyin.dmg.applescript.in"
        "${CMAKE_CURRENT_BINARY_DIR}/fooyin.dmg.applescript"
        @ONLY
    )

    set(FOOYIN_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/dist/macos/dmg-background.png" PARENT_SCOPE)
    set(FOOYIN_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/fooyin.dmg.applescript" PARENT_SCOPE)
endfunction()
