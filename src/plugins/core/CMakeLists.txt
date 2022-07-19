add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(utils)

target_include_directories(${PROJECT_NAME} PRIVATE .)
