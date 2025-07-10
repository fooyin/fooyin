#pragma once

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/plugins/guiplugin.h>

#include <systemmediatransportcontrolsinterop.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

namespace Fooyin {
struct PlaylistTrack;

namespace MediaControl {
class MediaControlPlugin : public QObject,
                           public Plugin,
                           public CorePlugin,
                           public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "mediacontrol.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    MediaControlPlugin();
    ~MediaControlPlugin() override;

    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

    void shutdown() override;

private:
    void trackChanged(const PlaylistTrack& playlistTrack);
    void playStateChanged();
    void playModeChanged();
    void updateDisplay();
    void buttonPressed(const winrt::Windows::Media::ISystemMediaTransportControls& sender,
                       const winrt::Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs& args);

    PlayerController* m_playerController = nullptr;
    PlaylistHandler* m_playlistHandler = nullptr;
    std::shared_ptr<AudioLoader> m_audioLoader;
    SettingsManager* m_settings = nullptr;
    CoverProvider* m_coverProvider = nullptr;

    winrt::Windows::Media::ISystemMediaTransportControls m_smtc{nullptr};
    winrt::event_token m_buttonPressedToken;
};
} // namespace MediaControl
} // namespace Fooyin
