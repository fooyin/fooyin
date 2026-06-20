function(fooyin_resolve_plugin_selection)
    cmake_parse_arguments(
        PLUGIN
        ""
        "OUT_VAR;SELECTION"
        "KNOWN;DEFAULT"
        ${ARGN}
    )

    if(NOT PLUGIN_OUT_VAR)
        message(FATAL_ERROR "fooyin_resolve_plugin_selection requires OUT_VAR.")
    endif()

    set(fooyin_plugin_selection "${PLUGIN_SELECTION}")
    string(REPLACE "," ";" fooyin_plugin_selection "${fooyin_plugin_selection}")

    set(fooyin_plugins_to_build)
    set(fooyin_plugins_to_remove)
    set(fooyin_unknown_plugins)
    set(fooyin_has_positive_selection FALSE)
    set(fooyin_has_plugin_selection FALSE)
    set(fooyin_use_no_plugins FALSE)
    set(fooyin_use_all_plugins FALSE)

    foreach(fooyin_plugin IN LISTS fooyin_plugin_selection)
        string(STRIP "${fooyin_plugin}" fooyin_plugin)
        if(fooyin_plugin STREQUAL "")
            continue()
        endif()
        set(fooyin_has_plugin_selection TRUE)

        if(fooyin_plugin STREQUAL "all")
            set(fooyin_use_all_plugins TRUE)
            continue()
        endif()

        if(fooyin_plugin STREQUAL "none")
            set(fooyin_use_no_plugins TRUE)
            continue()
        endif()

        set(fooyin_remove_plugin FALSE)
        if(fooyin_plugin MATCHES "^-(.+)$")
            set(fooyin_plugin "${CMAKE_MATCH_1}")
            set(fooyin_remove_plugin TRUE)
        else()
            set(fooyin_has_positive_selection TRUE)
        endif()

        if(NOT fooyin_plugin IN_LIST PLUGIN_KNOWN)
            list(APPEND fooyin_unknown_plugins "${fooyin_plugin}")
            continue()
        endif()

        if(fooyin_remove_plugin)
            list(APPEND fooyin_plugins_to_remove "${fooyin_plugin}")
        else()
            list(APPEND fooyin_plugins_to_build "${fooyin_plugin}")
        endif()
    endforeach()

    if(fooyin_unknown_plugins)
        list(REMOVE_DUPLICATES fooyin_unknown_plugins)
        list(JOIN fooyin_unknown_plugins ", " fooyin_unknown_plugins_text)
        list(JOIN PLUGIN_KNOWN ", " fooyin_known_plugins_text)
        message(FATAL_ERROR "Unknown plugin(s) in PLUGIN_SELECTION: ${fooyin_unknown_plugins_text}. "
                            "Known plugins: ${fooyin_known_plugins_text}"
        )
    endif()

    if(fooyin_use_no_plugins
       AND (fooyin_use_all_plugins
            OR fooyin_has_positive_selection
            OR fooyin_plugins_to_remove)
    )
        message(FATAL_ERROR "PLUGIN_SELECTION keyword 'none' cannot be combined with other selections.")
    endif()

    if(fooyin_use_no_plugins)
        set(fooyin_resolved_plugins)
    elseif(fooyin_use_all_plugins)
        set(fooyin_resolved_plugins ${PLUGIN_KNOWN})
    elseif(fooyin_has_positive_selection)
        set(fooyin_resolved_plugins ${fooyin_plugins_to_build})
    else()
        set(fooyin_resolved_plugins ${PLUGIN_DEFAULT})
    endif()

    list(REMOVE_ITEM fooyin_resolved_plugins ${fooyin_plugins_to_remove})
    list(REMOVE_DUPLICATES fooyin_resolved_plugins)

    if(fooyin_has_plugin_selection)
        if(fooyin_use_no_plugins)
            set(fooyin_resolved_plugins_text "none")
        else()
            list(JOIN fooyin_resolved_plugins ", " fooyin_resolved_plugins_text)
        endif()
        message(STATUS "Plugin selection resolved to: ${fooyin_resolved_plugins_text}")
    endif()

    set(${PLUGIN_OUT_VAR}
        ${fooyin_resolved_plugins}
        PARENT_SCOPE
    )
endfunction()

function(fooyin_add_selected_plugin plugin_name)
    if(NOT plugin_name IN_LIST FOOYIN_RESOLVED_PLUGINS)
        return()
    endif()

    add_subdirectory(${plugin_name})
endfunction()
