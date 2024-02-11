if(LSB_RELEASE AND DPKG_BUILDPACKAGE)
    execute_process(
        COMMAND env LC_ALL=C date "+%a, %-d %b %Y %H:%M:%S %z"
        OUTPUT_VARIABLE RELEASE_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND /bin/sh "-c" "${LSB_RELEASE} -cs"
        OUTPUT_VARIABLE RELEASE_CODENAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(RELEASE_DATE AND RELEASE_CODENAME)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/changelog.in
            ${CMAKE_CURRENT_SOURCE_DIR}/changelog
        )
    endif()
endif()
