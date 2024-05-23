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
            "fooyin_${CPACK_PACKAGE_VERSION}-${DIST_RELEASE}_${CPACK_SYSTEM_NAME}.deb"
    )
endif()

if(CPACK_GENERATOR STREQUAL "RPM")
    find_program(RPM rpm)
    if(NOT RPM)
        message(STATUS "Cannot find rpm, defaulting to i386")
        set(CPACK_SYSTEM_NAME i386)
    else()
        execute_process(
                COMMAND "${RPM}" --eval %{_target_cpu}
                OUTPUT_VARIABLE CPACK_SYSTEM_NAME
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    find_program(LSB_RELEASE lsb_release)
    if(NOT LSB_RELEASE)
        message(FATAL_ERROR "lsb_release not found, required for cpack -G RPM")
    endif()

    execute_process(
            COMMAND env LC_ALL="en_US.utf8" date "+%a %b %d %Y"
            OUTPUT_VARIABLE RPM_DATE
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
            COMMAND
            /bin/sh "-c"
            "${LSB_RELEASE} -is | tr '[:upper:]' '[:lower:]' | cut -d' ' -f1"
            OUTPUT_VARIABLE DIST_NAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
            COMMAND
            /bin/sh "-c"
            "${LSB_RELEASE} -ds | tr '[:upper:]' '[:lower:]' | sed 's/\"//g' | cut -d' ' -f2"
            OUTPUT_VARIABLE DIST_RELEASE
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
            COMMAND
            /bin/sh "-c"
            "${LSB_RELEASE} -ds | tr '[:upper:]' '[:lower:]' | sed 's/\"//g' | sed 's/\\.//g' | cut -d' ' -f3"
            OUTPUT_VARIABLE DIST_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(DIST_NAME)
        if(${DIST_NAME} STREQUAL "opensuse" AND DIST_RELEASE)
            if(${DIST_RELEASE} STREQUAL "leap")
                if(DIST_VERSION)
                    set(RPM_DISTRO "lp${DIST_VERSION}")
                else()
                    set(RPM_DISTRO ${DIST_RELEASE})
                endif()
            elseif(${DIST_RELEASE} STREQUAL "tumbleweed")
                set(RPM_DISTRO ${DIST_RELEASE})
            endif()
        elseif(${DIST_NAME} STREQUAL "fedora" AND DIST_VERSION)
            set(RPM_DISTRO "fc${DIST_VERSION}")
        elseif(${DIST_NAME} STREQUAL "centos" AND DIST_VERSION)
            set(RPM_DISTRO "el${DIST_VERSION}")
        elseif(${DIST_NAME} STREQUAL "mageia" AND DIST_RELEASE)
            set(RPM_DISTRO "mga${DIST_RELEASE}")
        endif()

        if(NOT RPM_DISTRO)
            set(RPM_DISTRO ${DIST_NAME})
        endif()

        set(CPACK_RPM_FILE_NAME
                "fooyin-${CPACK_PACKAGE_VERSION}.${RPM_DISTRO}.${CPACK_SYSTEM_NAME}.rpm"
        )
    endif()
endif()
