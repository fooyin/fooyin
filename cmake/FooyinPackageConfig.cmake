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
        COMMAND ${LSB_RELEASE} --short --codename
        OUTPUT_VARIABLE DIST_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND env LC_ALL=C date "+%a, %-d %b %Y %H:%M:%S %z"
        OUTPUT_VARIABLE DEB_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(CPACK_DEBIAN_FILE_NAME "fooyin_${CPACK_PACKAGE_VERSION}-${DIST_RELEASE}_${CPACK_SYSTEM_NAME}.deb")

    # cmake-format: off

    # Distro specific runtime dependencies
    set(DISTRO_ICU_PACKAGE_MAP
        bookworm libicu72
        noble    libicu74
        trixie   libicu76
        questing libicu76
        forky    libicu78
        resolute libicu78
    )

    set(DISTRO_TAGLIB_PACKAGE_MAP
        bookworm libtag1v5
        noble    libtag1v5
        trixie   libtag2
        questing libtag2
        forky    libtag2
        resolute libtag2
    )

    set(DISTRO_QT_PACKAGE_MAP
        bookworm "libqt6core6 (>= 6.4.0), libqt6gui6 (>= 6.4.0), libqt6widgets6 (>= 6.4.0), libqt6network6 (>= 6.4.0), libqt6concurrent6 (>= 6.4.0), libqt6sql6 (>= 6.4.0)"
        noble    "libqt6core6t64 (>= 6.4.0), libqt6gui6t64 (>= 6.4.0), libqt6widgets6t64 (>= 6.4.0), libqt6network6t64 (>= 6.4.0), libqt6concurrent6t64 (>= 6.4.0), libqt6sql6t64 (>= 6.4.0)"
        trixie   "libqt6core6t64 (>= 6.4.0), libqt6gui6 (>= 6.4.0), libqt6widgets6 (>= 6.4.0), libqt6network6 (>= 6.4.0), libqt6concurrent6 (>= 6.4.0), libqt6sql6 (>= 6.4.0)"
        questing "libqt6core6t64 (>= 6.4.0), libqt6gui6 (>= 6.4.0), libqt6widgets6 (>= 6.4.0), libqt6network6 (>= 6.4.0), libqt6concurrent6 (>= 6.4.0), libqt6sql6 (>= 6.4.0)"
        forky    "libqt6core6t64 (>= 6.4.0), libqt6gui6 (>= 6.4.0), libqt6widgets6 (>= 6.4.0), libqt6network6 (>= 6.4.0), libqt6concurrent6 (>= 6.4.0), libqt6sql6 (>= 6.4.0)"
        resolute "libqt6core6t64 (>= 6.4.0), libqt6gui6 (>= 6.4.0), libqt6widgets6 (>= 6.4.0), libqt6network6 (>= 6.4.0), libqt6concurrent6 (>= 6.4.0), libqt6sql6 (>= 6.4.0)"
    )

    # cmake-format: on

    function(get_distro_package map_name distro out_var)
        list(FIND ${map_name} "${distro}" idx)
        if(idx GREATER_EQUAL 0)
            math(EXPR pkg_idx "${idx} + 1")
            list(GET ${map_name} ${pkg_idx} pkg)
            set(${out_var}
                "${pkg}"
                PARENT_SCOPE
            )
        else()
            set(${out_var}
                ""
                PARENT_SCOPE
            )
        endif()
    endfunction()

    get_distro_package(DISTRO_ICU_PACKAGE_MAP "${DIST_RELEASE}" ICU_PKG)
    get_distro_package(DISTRO_TAGLIB_PACKAGE_MAP "${DIST_RELEASE}" TAGLIB_PKG)
    get_distro_package(DISTRO_QT_PACKAGE_MAP "${DIST_RELEASE}" QT_PKGS)

    if(NOT QT_PKGS)
        message(
            FATAL_ERROR "Unsupported distribution '${DIST_RELEASE}', update Qt runtime dependencies for cpack -G DEB"
        )
    endif()

    foreach(pkg IN ITEMS "${ICU_PKG}" "${TAGLIB_PKG}" "${QT_PKGS}")
        if(pkg)
            string(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS ", ${pkg}")
        endif()
    endforeach()

    if(NOT "${DIST_RELEASE}" STREQUAL "bookworm")
        string(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS ", libqcoro6core0t64, libqcoro6network0t64")
    endif()

    function(validate_deb_runtime_dependencies dependencies)
        find_program(APT_CACHE apt-cache)
        if(NOT APT_CACHE)
            message(STATUS "apt-cache not found, skipping DEB runtime dependency validation")
            return()
        endif()

        string(REGEX REPLACE "[\r\n]+" " " dependency_list "${dependencies}")
        string(REPLACE "," ";" dependency_list "${dependency_list}")

        foreach(dependency IN LISTS dependency_list)
            string(STRIP "${dependency}" dependency)
            string(REGEX REPLACE " *\\(.*\\)$" "" dependency "${dependency}")
            string(REGEX REPLACE " *\\[.*\\]$" "" dependency "${dependency}")
            string(REGEX REPLACE " *\\|.*$" "" dependency "${dependency}")
            string(STRIP "${dependency}" dependency)

            if(dependency)
                execute_process(
                    COMMAND "${APT_CACHE}" show "${dependency}"
                    RESULT_VARIABLE dependency_result
                    OUTPUT_QUIET ERROR_QUIET
                )

                if(NOT dependency_result EQUAL 0)
                    message(
                        FATAL_ERROR
                            "DEB runtime dependency '${dependency}' not found for ${DIST_RELEASE}/${CPACK_SYSTEM_NAME}"
                    )
                endif()
            endif()
        endforeach()
    endfunction()

    validate_deb_runtime_dependencies("${CPACK_DEBIAN_PACKAGE_DEPENDS}")
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
        COMMAND /bin/sh "-c" "${LSB_RELEASE} -is | tr '[:upper:]' '[:lower:]' | cut -d' ' -f1"
        OUTPUT_VARIABLE DIST_NAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND /bin/sh "-c" "${LSB_RELEASE} -ds | tr '[:upper:]' '[:lower:]' | sed 's/\"//g' | cut -d' ' -f2"
        OUTPUT_VARIABLE DIST_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND /bin/sh "-c"
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

        set(CPACK_RPM_FILE_NAME "fooyin-${CPACK_PACKAGE_VERSION}.${RPM_DISTRO}.${CPACK_SYSTEM_NAME}.rpm")
    endif()
endif()
