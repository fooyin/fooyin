# ---- Fooyin data ----

install(
    FILES "${CMAKE_SOURCE_DIR}/COPYING"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/fooyin
    RENAME LICENSE
)

install(
    FILES "${CMAKE_SOURCE_DIR}/README.md"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/fooyin
    RENAME README
)

install(FILES ${CMAKE_SOURCE_DIR}/dist/fooyin.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
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
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${SIZE}x${SIZE}/apps
        RENAME fooyin.png
    )
endforeach(SIZE)

install(
    FILES ${ICON_SRC_PATH}/sc-fooyin.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
    RENAME fooyin.svg
)

# ---- Fooyin executable ----

install(TARGETS fooyin_exe RUNTIME COMPONENT fooyin_runtime)

# ---- Fooyin config ----

set(package Fooyin)

install(
    TARGETS fooyin_lib fooyin_version fooyin_config
    EXPORT FooyinTargets
    RUNTIME COMPONENT fooyin_runtime
    LIBRARY COMPONENT fooyin_runtime NAMELINK_COMPONENT fooyin_development
    ARCHIVE COMPONENT fooyin_development
    INCLUDES
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/FooyinConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

set(FOOYIN_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/fooyin"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(FOOYIN_INSTALL_CMAKEDIR)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/FooyinConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/FooyinConfig.cmake"
    INSTALL_DESTINATION ${FOOYIN_INSTALL_CMAKEDIR}
    PATH_VARS FOOYIN_PLUGIN_RPATH
              FOOYIN_PLUGIN_INSTALL_DIR
)

install(
    FILES "${PROJECT_BINARY_DIR}/FooyinConfig.cmake"
    DESTINATION "${FOOYIN_INSTALL_CMAKEDIR}"
    COMPONENT fooyin_development
)

install(
    FILES "${PROJECT_BINARY_DIR}/FooyinConfigVersion.cmake"
    DESTINATION "${FOOYIN_INSTALL_CMAKEDIR}"
    COMPONENT fooyin_development
)

install(
    FILES "${CMAKE_CURRENT_LIST_DIR}/FooyinMacros.cmake"
    DESTINATION "${FOOYIN_INSTALL_CMAKEDIR}"
    COMPONENT fooyin_development
)

install(
    EXPORT FooyinTargets
    DESTINATION "${FOOYIN_INSTALL_CMAKEDIR}"
    NAMESPACE Fooyin::
    COMPONENT fooyin_development
)

install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/modules/"
        DESTINATION ${FOOYIN_INSTALL_CMAKEDIR}/modules
)

# ---- Fooyin public headers ----

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
    DESTINATION ${FOOYIN_INCLUDE_INSTALL_DIR}
    COMPONENT fooyin_development
)
