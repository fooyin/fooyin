set(SOURCES
    globalhotkeysplugin.cpp
    globalhotkeysplugin.h
    globalshortcutbackend.h
    globalshortcutkeyutils.cpp
    globalshortcutkeyutils.h
    globalshortcutmanager.cpp
    globalshortcutmanager.h
    globalshortcutrepeater.cpp
    globalshortcutrepeater.h
)

set(DEPENDS Fooyin::Gui)

if(UNIX AND NOT APPLE)
    find_package(Qt6 ${FOOYIN_QT_MIN_VERSION} QUIET COMPONENTS DBus)
    find_package(X11 QUIET)

    if(TARGET Qt6::DBus)
        list(
            APPEND
            SOURCES
            globalshortcutportalbackend.cpp
            globalshortcutportalbackend.h
            globalshortcutportaltypes.cpp
            globalshortcutportaltypes.h
        )
        list(APPEND DEPENDS Qt6::DBus)
    endif()

    if(X11_FOUND)
        list(APPEND SOURCES globalshortcutx11backend.cpp globalshortcutx11backend.h)
        list(APPEND DEPENDS X11::X11)
    endif()
elseif(WIN32)
    list(APPEND SOURCES globalshortcutwindowsbackend.cpp globalshortcutwindowsbackend.h)
    list(APPEND DEPENDS user32)
elseif(APPLE)
    find_library(CARBON_FRAMEWORK Carbon REQUIRED)
    list(APPEND SOURCES globalshortcutmacosbackend.cpp globalshortcutmacosbackend.h)
    list(APPEND DEPENDS ${CARBON_FRAMEWORK})
endif()

create_fooyin_plugin_internal(
    globalhotkeys
    DEPENDS ${DEPENDS}
    SOURCES ${SOURCES}
)

if(UNIX
   AND NOT APPLE
   AND TARGET Qt6::DBus
)
    target_compile_definitions(globalhotkeys PRIVATE FOOYIN_HAVE_GLOBAL_SHORTCUT_PORTAL)
elseif(WIN32)
    target_compile_definitions(globalhotkeys PRIVATE FOOYIN_HAVE_WINDOWS_GLOBAL_SHORTCUTS)
elseif(APPLE)
    target_compile_definitions(globalhotkeys PRIVATE FOOYIN_HAVE_MACOS_GLOBAL_SHORTCUTS)
elseif(X11_FOUND)
    target_compile_definitions(globalhotkeys PRIVATE FOOYIN_HAVE_X11_GLOBAL_SHORTCUTS)
endif()
