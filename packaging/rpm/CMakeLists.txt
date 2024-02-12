if(LSB_RELEASE AND RPMBUILD)
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
        set(RPMBUILD_DIR "~/rpmbuild")
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

        configure_file(
            "${CMAKE_SOURCE_DIR}/packaging/rpm/fooyin.spec.in"
            "${CMAKE_SOURCE_DIR}/packaging/rpm/fooyin.spec" @ONLY
        )

        add_custom_target(
            rpm
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND ${CMAKE_SOURCE_DIR}/packaging/packagetar.sh
            COMMAND ${CMAKE_COMMAND} -E make_directory ${RPMBUILD_DIR}/SOURCES/
            COMMAND
                ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/fooyin-${FOOYIN_VERSION}.tar.xz
                ${RPMBUILD_DIR}/SOURCES/
            COMMAND ${RPMBUILD} -ba
                    ${CMAKE_SOURCE_DIR}/packaging/rpm/fooyin.spec
        )
    endif()
endif()
