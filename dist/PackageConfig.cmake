set(CMAKE_CURRENT_SOURCE_DIR "${CPACK_SOURCE_DIR}")

set(CPACK_PACKAGE_FILE_NAME "fooyin_${CPACK_PACKAGE_VERSION}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-src")

if(CPACK_GENERATOR STREQUAL "DEB")
    find_program(DPKG dpkg)
    if(NOT DPKG)
        message(STATUS "Cannot find dpkg, defaulting to i386")
        set(CPACK_SYSTEM_NAME i386)
    else()
        execute_process(
            COMMAND "${DPKG}" --print-architecture
            OUTPUT_VARIABLE CPACK_SYSTEM_NAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    find_program(LSB_RELEASE lsb_release)
    if(NOT LSB_RELEASE)
        message(FATAL_ERROR "lsb_release not found, required for cpack -G DEB")
    endif()

    execute_process(
        COMMAND "${LSB_RELEASE} -is | tr '[:upper:]' '[:lower:]' | cut -d' ' -f1"
        OUTPUT_VARIABLE DIST_NAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND ${LSB_RELEASE} --short --codename
        OUTPUT_VARIABLE DIST_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND env LC_ALL=C date "+%a, %-d %b %Y %H:%M:%S %z"
        OUTPUT_VARIABLE DEB_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(CPACK_DEBIAN_FILE_NAME
        "${CPACK_PACKAGE_FILE_NAME}-${DIST_RELEASE}_${CPACK_SYSTEM_NAME}.deb"
    )
    set(CPACK_INSTALL_SCRIPT ${CPACK_DEBIAN_INSTALL_SCRIPT})
endif()
