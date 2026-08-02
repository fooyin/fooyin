find_package(
    Qt6
    COMPONENTS OpenGL
    QUIET
)

if(NOT Qt6OpenGL_FOUND)
    message(STATUS "Qt6::OpenGL not found; skipping projectm plugin.")
    return()
endif()

set(PROJECTM_TARGET)
set(PROJECTM_OPENGL_LOADER_TARGET)

include("${PROJECT_SOURCE_DIR}/3rdparty/projectm.cmake")

if(TARGET libprojectM::projectM AND TARGET libprojectM::playlist)
    set(PROJECTM_TARGET libprojectM::projectM libprojectM::playlist)
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
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Qt6::OpenGL
            ${PROJECTM_TARGET}
            ${PROJECTM_OPENGL_LOADER_TARGET}
    SOURCES projectminstance.cpp
            projectminstance.h
            projectmconfigdialog.cpp
            projectmconfigdialog.h
            projectmplugin.cpp
            projectmplugin.h
            projectmwidget.cpp
            projectmwidget.h
            projectmsettings.h
            projectmpreset.h
            projectmpresetlibrary.cpp
            projectmpresetlibrary.h
            projectmpresetdialog.cpp
            projectmpresetdialog.h
            projectmview.cpp
            projectmview.h
)
