find_package(OpenMpt QUIET)

if(NOT OPENMPT_FOUND)
    message(STATUS "OpenMPT not found; skipping openmpt plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    openmpt
    DEPENDS Fooyin::Core
            ${OPENMPT_LIBRARIES}
    SOURCES openmptinput.cpp
            openmptinput.h
            openmptplugin.cpp
            openmptplugin.h
)

target_include_directories(
    openmpt PRIVATE ${OPENMPT_INCLUDE_DIRS}
)
