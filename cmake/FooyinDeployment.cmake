find_program(LSB_RELEASE lsb_release)
find_program(DPKG_BUILDPACKAGE dpkg-buildpackage)

if (LSB_RELEASE AND DPKG_BUILDPACKAGE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian")
    add_custom_target(deb WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ln -sn "packaging/debian" "${CMAKE_CURRENT_SOURCE_DIR}/debian"
            && ${DPKG_BUILDPACKAGE} -b -d -uc -us -nc -jauto)
endif()

find_program(RPMBUILD rpmbuild)

if (LSB_RELEASE AND RPMBUILD)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/packaging/packagetar.sh.in" "${CMAKE_CURRENT_SOURCE_DIR}/packaging/packagetar.sh" @ONLY)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/packaging/rpm")
endif()