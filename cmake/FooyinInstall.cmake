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
