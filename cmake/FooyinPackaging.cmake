set(CPACK_PACKAGE_NAME "fooyin")
set(CPACK_PACKAGE_VENDOR "fooyin")
set(CPACK_PACKAGE_CONTACT "Luke Taylor <luket@pm.me>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A customisable music player")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_NAME})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_VERSION_MAJOR "${FOOYIN_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${FOOYIN_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${FOOYIN_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION "${FOOYIN_VERSION}")
set(CPACK_PACKAGE_RELOCATABLE ON CACHE BOOL "Build relocatable package")
set(CPACK_PACKAGE_RELEASE 1)

if(WIN32)
    set(CPACK_GENERATOR "NSIS" "ZIP")
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/data/icons\\\\fooyin.ico")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/data/icons\\\\fooyin.ico")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://www.fooyin.org/")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "${CPACK_PACKAGE_NAME}{CMAKE_EXECUTABLE_SUFFIX}")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${CPACK_PACKAGE_NAME}.exe")
    set(CPACK_COMPONENTS_ALL fooyin)

    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "${BIN_INSTALL_DIR}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "fooyin")
    set(CPACK_INSTALL_SCRIPTS "${CMAKE_SOURCE_DIR}/cmake/FooyinPostInstall.cmake")
else()
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/data/icons/sc-fooyin.svg")
endif()

set(CPACK_SOURCE_IGNORE_FILES  "\\\\.#;/#;.*~;\\\\.o$")
list(APPEND CPACK_SOURCE_IGNORE_FILES "/\\\\.git/")
list(APPEND CPACK_SOURCE_IGNORE_FILES "/\\\\.github/")
list(APPEND CPACK_SOURCE_IGNORE_FILES "/build/")
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_BINARY_DIR}/")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Luke Taylor <luket@pm.me>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${CPACK_PROJECT_URL})
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libc6,
        libstdc++6,
        libqt6core6 (>= 6.4.0),
        libqt6gui6 (>= 6.4.0),
        libqt6widgets6 (>= 6.4.0),
        libqt6network6 (>= 6.4.0),
        libqt6concurrent6 (>= 6.4.0),
        libqt6sql6 (>= 6.4.0),
        libqt6sql6-sqlite (>= 6.4.0),
        libqt6svg6 (>= 6.4.0),
        qt6-image-formats-plugins (>= 6.4.0),
        qt6-qpa-plugins,
        libasound2,
        ffmpeg (>= 7:4.4),
        pipewire,
        libsdl2-2.0-0,
        libgme0,
        libopenmpt0,
        libarchive13,
        libsndfile1,
        libebur128-1"
)

set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
SET(CPACK_RPM_PACKAGE_REQUIRES_POST "qt6-qtimageformats")

set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/cmake/FooyinPackageConfig.cmake" )

include(CPack)
