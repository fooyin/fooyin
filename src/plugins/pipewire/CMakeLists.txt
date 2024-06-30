find_package(PipeWire QUIET)

if(NOT PIPEWIRE_FOUND)
    message(STATUS "PipeWire not found; skipping pipewire plugin.")
    return()
endif()

create_fooyin_plugin_internal(
    pipewire
    DEPENDS Fooyin::Core
            PipeWire::PipeWire
    SOURCES pipewireplugin.cpp
            pipewireplugin.h
            pipewireoutput.cpp
            pipewireoutput.h
            pipewirecontext.cpp
            pipewirecontext.h
            pipewirecore.cpp
            pipewirecore.h
            pipewireregistry.cpp
            pipewireregistry.h
            pipewirestream.cpp
            pipewirestream.h
            pipewirethreadloop.cpp
            pipewirethreadloop.h
)
