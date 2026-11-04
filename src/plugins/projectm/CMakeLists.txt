find_package(Qt6 COMPONENTS OpenGL QUIET)

if(NOT Qt6OpenGL_FOUND)
    message(STATUS "Qt6::OpenGL not found; skipping projectm plugin.")
    return()
endif()

set(PROJECTM_TARGET)
set(PROJECTM_OPENGL_LOADER_TARGET)

include("${PROJECT_SOURCE_DIR}/3rdparty/projectm.cmake")

if(TARGET libprojectM::projectM AND TARGET libprojectM::playlist)
    set(PROJECTM_TARGET libprojectM::playlist)
else()
    message(STATUS "projectM 4 not found; skipping projectm plugin.")
    return()
endif()

if(WIN32)
    if(TARGET GLEW::glew)
        set(PROJECTM_OPENGL_LOADER_TARGET GLEW::glew)
    elseif(TARGET GLEW::glew_s)
        set(PROJECTM_OPENGL_LOADER_TARGET GLEW::glew_s)
    else()
        message(STATUS "GLEW not found; skipping projectm plugin on Windows.")
        return()
    endif()
endif()

create_fooyin_plugin_internal(
    projectm
    DEPENDS #
        Fooyin::Core
        Fooyin::Gui
        Fooyin::GuiPrivate
        Qt6::OpenGL
        ${PROJECTM_TARGET}
        ${PROJECTM_OPENGL_LOADER_TARGET}
    SOURCES
        projectmconfigdialog.cpp
        projectmconfigdialog.h
        projectmfavouritestore.cpp
        projectmfavouritestore.h
        projectminstance.cpp
        projectminstance.h
        projectmplugin.cpp
        projectmplugin.h
        projectmpreset.h
        projectmpresetdialog.cpp
        projectmpresetdialog.h
        projectmpresetlibrary.cpp
        projectmpresetlibrary.h
        projectmsettings.h
        projectmview.cpp
        projectmview.h
        projectmwidget.cpp
        projectmwidget.h
)
