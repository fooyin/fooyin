include(FooyinPluginSelection)

set(
    FOOYIN_KNOWN_PLUGINS
    alsa
    discord
    equaliser
    fileops
    filters
    gme
    libarchive
    lyrics
    mediacontrol
    mpris
    notify
    openmpt
    pipewire
    rawaudio
    rgscanner
    scrobbler
    sdl
    sndfile
    soundtouch
    soxresampler
    tageditor
    thumbnailtoolbar
    vumeter
    wavebar
)

set(
    FOOYIN_DEFAULT_PLUGINS
    discord
    equaliser
    fileops
    filters
    gme
    libarchive
    lyrics
    openmpt
    pipewire
    rawaudio
    rgscanner
    scrobbler
    sdl
    sndfile
    soundtouch
    soxresampler
    tageditor
    vumeter
    wavebar
)

if(BUILD_ALSA)
    list(APPEND FOOYIN_DEFAULT_PLUGINS alsa)
endif()

if(UNIX AND NOT APPLE)
    list(APPEND FOOYIN_DEFAULT_PLUGINS mpris notify)
endif()

if(WIN32)
    list(APPEND FOOYIN_DEFAULT_PLUGINS mediacontrol thumbnailtoolbar)
endif()

fooyin_resolve_plugin_selection(
    OUT_VAR FOOYIN_RESOLVED_PLUGINS
    SELECTION "${PLUGIN_SELECTION}"
    KNOWN ${FOOYIN_KNOWN_PLUGINS}
    DEFAULT ${FOOYIN_DEFAULT_PLUGINS}
)

if(BUILD_ALSA)
    fooyin_add_selected_plugin(alsa)
elseif("alsa" IN_LIST FOOYIN_RESOLVED_PLUGINS)
    message(STATUS "BUILD_ALSA=OFF; skipping alsa plugin.")
endif()

if(UNIX AND NOT APPLE)
    fooyin_add_selected_plugin(mpris)
    fooyin_add_selected_plugin(notify)
else()
    if("mpris" IN_LIST FOOYIN_RESOLVED_PLUGINS)
        message(STATUS "mpris plugin is only available on Unix platforms other than macOS; skipping.")
    endif()
    if("notify" IN_LIST FOOYIN_RESOLVED_PLUGINS)
        message(STATUS "notify plugin is only available on Unix platforms other than macOS; skipping.")
    endif()
endif()

fooyin_add_selected_plugin(discord)
fooyin_add_selected_plugin(equaliser)
fooyin_add_selected_plugin(fileops)
fooyin_add_selected_plugin(filters)
fooyin_add_selected_plugin(gme)
fooyin_add_selected_plugin(libarchive)
fooyin_add_selected_plugin(lyrics)
fooyin_add_selected_plugin(openmpt)
fooyin_add_selected_plugin(pipewire)
fooyin_add_selected_plugin(rawaudio)
fooyin_add_selected_plugin(rgscanner)
fooyin_add_selected_plugin(sdl)
fooyin_add_selected_plugin(scrobbler)
fooyin_add_selected_plugin(sndfile)
fooyin_add_selected_plugin(tageditor)
fooyin_add_selected_plugin(soundtouch)
fooyin_add_selected_plugin(soxresampler)
fooyin_add_selected_plugin(vumeter)
fooyin_add_selected_plugin(wavebar)

if(WIN32)
    fooyin_add_selected_plugin(mediacontrol)
    fooyin_add_selected_plugin(thumbnailtoolbar)
else()
    if("mediacontrol" IN_LIST FOOYIN_RESOLVED_PLUGINS)
        message(STATUS "mediacontrol plugin is only available on Windows; skipping.")
    endif()
    if("thumbnailtoolbar" IN_LIST FOOYIN_RESOLVED_PLUGINS)
        message(STATUS "thumbnailtoolbar plugin is only available on Windows; skipping.")
    endif()
endif()