find_program(DEBHELPER dh_prep)
if(NOT DEBHELPER)
    message(FATAL_ERROR "debhelper not found, required for cpack -G DEB")
endif()

find_program(DEBCHANGE debchange)
if(NOT DEBCHANGE)
    message(FATAL_ERROR "debchange not found, required for cpack -G DEB")
endif()

file(COPY ${CPACK_DEBIAN_SOURCE_DIR}/dist/linux/debian
     DESTINATION ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}
)
configure_file(
    ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/debian/control.in
    ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/debian/control @ONLY
)
configure_file(
    ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/debian/changelog.in
    ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/debian/changelog
)
file(REMOVE
     ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/debian/changelog.in
)

execute_process(
    COMMAND
        ${DEBCHANGE} -v
        "${CPACK_DEBIAN_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}" -M
        "Build of ${CPACK_DEBIAN_PACKAGE_VERSION}"
    WORKING_DIRECTORY ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}
)
execute_process(
    COMMAND ${DEBCHANGE} -r -M "Build of ${CPACK_DEBIAN_PACKAGE_VERSION}"
    WORKING_DIRECTORY ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}
)

function(dh_run command)
    execute_process(
        COMMAND ${command} ${ARGV1} ${ARGV2} -P.
        WORKING_DIRECTORY ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}
        RESULT_VARIABLE DH_RET
    )
    if(NOT DH_RET EQUAL "0")
        message(FATAL_ERROR "${command} returned exit code ${DH_RET}")
    endif()
endfunction()

dh_run(dh_testdir)
dh_run(dh_installdocs)
dh_run(dh_installchangelogs)

file(REMOVE_RECURSE ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/debian)
file(REMOVE ${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/fooyin.1)
