create_fooyin_plugin_internal(
    discord
    DEPENDS Fooyin::Core
            Fooyin::Gui
            QCoro6::Network
    SOURCES discordmessage.cpp
            discordmessage.h
            discordipcclient.cpp
            discordipcclient.h
            discordplugin.cpp
            discordplugin.h
            discordpresencedata.h
            settings/discordpage.cpp
            settings/discordpage.h
            settings/discordsettings.cpp
            settings/discordsettings.h
)
