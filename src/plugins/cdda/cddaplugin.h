/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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
 */

#pragma once

#include <core/engine/inputplugin.h>
#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <core/track.h>
#include <gui/plugins/guiplugin.h>

#include <memory>

namespace Fooyin {
class NetworkAccessManager;
class PlayerController;
class Playlist;
class PlaylistHandler;
class SettingsManager;
class ConversionService;
class CurrentPlaylistController;

namespace Cdda {
class AccurateRipVerifier;
class CdDriveManager;
class CdDriveSettingsStore;
class RipAudioCdDialog;
struct CdToc;
struct CdDriveObservation;

class CddaPlugin : public QObject,
                   public Plugin,
                   public CorePlugin,
                   public InputPlugin,
                   public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin" FILE "cdda.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::InputPlugin Fooyin::GuiPlugin)

public:
    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

    [[nodiscard]] QString inputName() const override;
    [[nodiscard]] InputCreator inputCreator() const override;

private:
    void addToPlaylist(const TrackList& tracks, const CdDriveObservation& observation);
    void play(const TrackList& tracks, const CdDriveObservation& observation);
    void rip(const TrackList& tracks, const CdDriveObservation& observation);
    void startRip(RipAudioCdDialog* dialog, TrackList tracks, CdToc toc, QString presetId, bool showSetup,
                  bool verifyAccurateRip);
    void startConversion(RipAudioCdDialog* dialog, const TrackList& tracks, const QString& presetId, bool showSetup,
                         std::shared_ptr<AccurateRipVerifier> verifier, QString lookupMessage = {});

    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;
    CurrentPlaylistController* m_playlistController;
    ConversionService* m_conversionService;
    std::shared_ptr<NetworkAccessManager> m_networkAccess;
    SettingsManager* m_settingsManager;

    std::shared_ptr<CdDriveManager> m_driveManager;
    std::shared_ptr<CdDriveSettingsStore> m_settingsStore;
};
} // namespace Cdda
} // namespace Fooyin
