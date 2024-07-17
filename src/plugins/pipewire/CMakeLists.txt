find_package(PipeWire QUIET)

if(NOT PIPEWIRE_FOUND)
    message(STATUS "PipeWire not found; skipping pipewire plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    pipewire
    DEPENDS Fooyin::Core
            PipeWire::PipeWire
    SOURCES pipewirecontext.cpp
            pipewirecontext.h
            pipewirecore.cpp
            pipewirecore.h
            pipewireoutput.cpp
            pipewireoutput.h
            pipewireplugin.cpp
            pipewireplugin.h
            pipewireregistry.cpp
            pipewireregistry.h
            pipewirestream.cpp
            pipewirestream.h
            pipewirethreadloop.cpp
            pipewirethreadloop.h
)
