add_subdirectory(utils)
add_subdirectory(core)
add_subdirectory(gui)

if(BUILD_PLUGINS)
    add_subdirectory(plugins)
endif()
