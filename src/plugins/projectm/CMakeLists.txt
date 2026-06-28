find_package(
    Qt6
    COMPONENTS OpenGL OpenGLWidgets
    QUIET
)

if(NOT Qt6OpenGL_FOUND OR NOT Qt6OpenGLWidgets_FOUND)
    message(STATUS "Qt6::OpenGL or Qt6::OpenGLWidgets not found; skipping projectm plugin.")
    return()
endif()

set(PROJECTM_TARGET)

include("${PROJECT_SOURCE_DIR}/3rdparty/projectm.cmake")

if(TARGET libprojectM::projectM AND TARGET libprojectM::playlist)
    set(PROJECTM_TARGET libprojectM::projectM libprojectM::playlist)
else()
    message(STATUS "projectM 4 not found; skipping projectm plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    projectm
    DEPENDS Fooyin::Core
            Fooyin::Gui
            Qt6::OpenGL
            Qt6::OpenGLWidgets
            ${PROJECTM_TARGET}
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
