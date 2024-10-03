find_package(OpenMpt QUIET)

if(NOT OPENMPT_FOUND)
    message(STATUS "OpenMPT not found; skipping openmpt plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    openmpt
    DEPENDS Fooyin::Core
            Fooyin::Gui
            ${OPENMPT_LIBRARIES}
    SOURCES openmptdefs.h
            openmptinput.cpp
            openmptinput.h
            openmptplugin.cpp
            openmptplugin.h
            openmptsettings.cpp
            openmptsettings.h
)

target_include_directories(
    openmpt PRIVATE ${OPENMPT_INCLUDE_DIRS}
)
