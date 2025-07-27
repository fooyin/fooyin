/*
 * Fooyin
 * Copyright © 2025, Carter Li <zhangsongcui@live.cn>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

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
