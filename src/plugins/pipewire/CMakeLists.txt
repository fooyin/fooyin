find_package(PipeWire QUIET)

if(NOT PIPEWIRE_FOUND)
    return()
endif()

create_fooyin_plugin_internal(
    pipewire
    DEPENDS Fooyin::Core
            PipeWire::PipeWire
    SOURCES pipewireplugin.cpp
            pipewireoutput.cpp
)
