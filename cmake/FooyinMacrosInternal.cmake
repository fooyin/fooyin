include(FooyinMacros)

function(create_fooyin_plugin_internal base_name)
    create_fooyin_plugin(${ARGV})

    string(TOLOWER ${base_name} name)
    set(plugin_name "fooyin_${name}")

    set_target_properties(
        ${plugin_name}
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
                   LIBRARY_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
                   ARCHIVE_OUTPUT_DIRECTORY ${FOOYIN_PLUGIN_OUTPUT_DIR}
    )

    target_compile_features(${plugin_name} PUBLIC ${FOOYIN_REQUIRED_CXX_FEATURES})
    target_compile_options(${plugin_name} PRIVATE ${FOOYIN_COMPILE_OPTIONS})
endfunction()
