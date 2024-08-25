create_fooyin_plugin_internal(
    rawaudio
    DEPENDS Fooyin::Core
    SOURCES rawaudioinput.cpp
            rawaudioinput.h
            rawaudioplugin.cpp
            rawaudioplugin.h
)

fooyin_append_mimes(
    application/octet-stream
)
