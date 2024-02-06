# ---- Fooyin data ----

install(
    FILES "${CMAKE_SOURCE_DIR}/COPYING"
    DESTINATION ${DOC_INSTALL_DIR}
    RENAME LICENSE
)

install(
    FILES "${CMAKE_SOURCE_DIR}/README.md"
    DESTINATION ${DOC_INSTALL_DIR}
    RENAME README
)

install(FILES ${CMAKE_SOURCE_DIR}/dist/fooyin.desktop
        DESTINATION ${XDG_APPS_INSTALL_DIR}
)
set(ICON_SRC_PATH ${CMAKE_SOURCE_DIR}/data/icons)
set(ICON_SIZE
    16
    22
    32
    48
    64
    128
    256
    512
)
foreach(SIZE ${ICON_SIZE})
    install(
        FILES ${ICON_SRC_PATH}/${SIZE}-fooyin.png
        DESTINATION ${ICON_INSTALL_DIR}/hicolor/${SIZE}x${SIZE}/apps
        RENAME fooyin.png
    )
endforeach(SIZE)

install(
    FILES ${ICON_SRC_PATH}/sc-fooyin.svg
    DESTINATION ${ICON_INSTALL_DIR}/hicolor/scalable/apps
    RENAME fooyin.svg
)

install(FILES ${TRANSLATIONS} DESTINATION ${TRANSLATION_INSTALL_DIR})

# ---- Fooyin executable ----

install(TARGETS fooyin RUNTIME COMPONENT fooyin_runtime)

# ---- Fooyin config ----

set(package Fooyin)

install(
    TARGETS fooyin_lib fooyin_version fooyin_config
    EXPORT FooyinTargets
    RUNTIME COMPONENT fooyin_runtime
    LIBRARY COMPONENT fooyin_runtime NAMELINK_COMPONENT fooyin_development
    ARCHIVE COMPONENT fooyin_development
    INCLUDES
    DESTINATION "${INCLUDE_INSTALL_DIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/FooyinConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/FooyinConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/FooyinConfig.cmake"
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
    PATH_VARS FOOYIN_PLUGIN_INSTALL_DIR
)

install(
    FILES "${PROJECT_BINARY_DIR}/FooyinConfig.cmake"
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    COMPONENT fooyin_development
)

install(
    FILES "${PROJECT_BINARY_DIR}/FooyinConfigVersion.cmake"
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    COMPONENT fooyin_development
)

install(
    FILES "${CMAKE_CURRENT_LIST_DIR}/FooyinMacros.cmake"
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    COMPONENT fooyin_development
)

install(
    EXPORT FooyinTargets
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    NAMESPACE Fooyin::
    COMPONENT fooyin_development
)

install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/modules/"
        DESTINATION ${CMAKECONFIG_INSTALL_DIR}/modules
)

# ---- Fooyin public headers ----

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
    DESTINATION ${INCLUDE_INSTALL_DIR}
    COMPONENT fooyin_development
)
