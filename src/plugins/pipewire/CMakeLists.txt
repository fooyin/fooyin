find_package(PipeWire)

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
