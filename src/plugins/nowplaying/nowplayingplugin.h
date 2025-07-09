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

namespace Fooyin {
struct PlaylistTrack;

namespace NowPlaying {
class NowPlayingPlugin : public QObject,
                         public Plugin,
                         public CorePlugin,
                         public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "nowplaying.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    NowPlayingPlugin();
    ~NowPlayingPlugin() override;

    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

    void shutdown() override;

    PlayerController* playerController() const
    {
        return m_playerController;
    }
    SettingsManager* settings() const
    {
        return m_settings;
    }

private:
    void setupRemoteCommands();
    void updateNowPlayingInfo();
    void trackChanged(const PlaylistTrack& playlistTrack);
    void playStateChanged();
    void positionMoved(uint64_t ms);

    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;
    std::shared_ptr<AudioLoader> m_audioLoader;
    SettingsManager* m_settings;
    CoverProvider* m_coverProvider;

    void* m_nowPlayingInfo; // NSMutableDictionary*
    void* m_remoteTarget;   // NowPlayingRemoteTarget*
};
} // namespace NowPlaying
} // namespace Fooyin
