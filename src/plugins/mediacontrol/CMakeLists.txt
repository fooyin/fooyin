if(WIN32)
    message(STATUS "On Windows, enabling mediacontrol plugin")

    set(SOURCES
        mediacontrolplugin.cpp
        mediacontrolplugin.h
    )

    create_fooyin_plugin_internal(
        mediacontrol
        DEPENDS Fooyin::Gui
        SOURCES ${SOURCES}
    )

    target_link_libraries(mediacontrol PRIVATE "RuntimeObject")
    target_compile_definitions(mediacontrol PRIVATE WINRT_LEAN_AND_MEAN=1)
endif()
