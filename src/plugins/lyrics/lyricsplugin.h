/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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
#include <gui/plugins/guiplugin.h>

namespace Fooyin {
class FyWidget;
class PlaylistHandler;
class Track;
class TrackSelectionController;

namespace Lyrics {
class LyricsFinder;
class LyricsSaver;
class LyricsSettings;
struct Lyrics;

class LyricsPlugin : public QObject,
                     public Plugin,
                     public CorePlugin,
                     public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "lyrics.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;
    void shutdown() override;

private:
    [[nodiscard]] Track selectedTrack() const;
    void openSearchDialog(const Track& track) const;
    void quickSearchLyrics(const Track& track);
    void openLyricsDialog(const Track& track, const Lyrics& lyrics);
    void loadTrackLyricsAndOpenDialog(const Track& track);

    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::shared_ptr<NetworkAccessManager> m_networkAccess;
    TrackSelectionController* m_trackSelection;
    WidgetProvider* m_widgetProvider;
    SettingsManager* m_settings;

    std::unique_ptr<LyricsSettings> m_lyricsSettings;
    LyricsFinder* m_lyricsFinder;
    LyricsSaver* m_lyricsSaver;
};
} // namespace Lyrics
} // namespace Fooyin
