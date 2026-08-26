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
 *
 */

#pragma once

#include "cddadrivemanager.h"

#include <core/track.h>

#include <QDialog>
#include <QPointer>

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

class QComboBox;
class QAction;
class QPushButton;
class QTableWidget;

namespace Fooyin {
class ElidedLabel;
class MetadataLookupDialog;
class MusicBrainzMetadata;
class NetworkAccessManager;
class SettingsManager;

namespace Cdda {
class CdDriveSettingsStore;

class OpenAudioCdDialog final : public QDialog
{
    Q_OBJECT

public:
    OpenAudioCdDialog(std::shared_ptr<CdDriveManager> driveManager, std::shared_ptr<CdDriveSettingsStore> settingsStore,
                      std::shared_ptr<NetworkAccessManager> networkAccess, SettingsManager* settingsManager,
                      QWidget* parent = nullptr);

Q_SIGNALS:
    void addRequested(const Fooyin::TrackList& tracks, const Fooyin::Cdda::CdDriveObservation& observation);
    void playRequested(const Fooyin::TrackList& tracks, const Fooyin::Cdda::CdDriveObservation& observation);
    void ripRequested(const Fooyin::TrackList& tracks, const Fooyin::Cdda::CdDriveObservation& observation);

private:
    struct DriveState
    {
        CdDriveInfo drive;
        std::optional<CdDriveObservation> observation;
    };

    void enumerateDrives();
    void refreshCurrentDrive();

    void showObservation(int index);
    void loadCdText(int index);
    void startAutomaticLookup(int index);
    void fetchNextAutomaticRelease(MusicBrainzMetadata* client);
    void cancelAutomaticLookup();
    void finishAutomaticLookup(MusicBrainzMetadata* client);

    void applyLookupTracks(const TrackList& tracks);
    void replaceTracks(const TrackList& tracks, const QString& status);

    void setStatus(const QString& status);
    void updateActions();

    [[nodiscard]] TrackList checkedTracks() const;

    void showMetadataLookup();
    void addToPlaylist();
    void play();
    void rip();
    void showDriveSettings();

    std::shared_ptr<CdDriveManager> m_driveManager;
    std::shared_ptr<CdDriveSettingsStore> m_settingsStore;
    std::shared_ptr<NetworkAccessManager> m_networkAccess;
    SettingsManager* m_settingsManager;

    uint64_t m_driveRequestId;
    std::vector<DriveState> m_driveStates;
    TrackList m_tracks;
    std::unordered_set<QString> m_cdTextLoads;
    QComboBox* m_drives;
    ElidedLabel* m_status;
    QTableWidget* m_trackTable;
    QPushButton* m_refreshButton;
    QPushButton* m_settingsButton;
    QPushButton* m_metadataButton;
    QAction* m_cdTextAction;
    QAction* m_autoLookupAction;
    QAction* m_lookupAction;
    QPushButton* m_ripButton;
    QPushButton* m_playButton;
    QPushButton* m_addButton;
    QPointer<MetadataLookupDialog> m_lookupDialog;
    QPointer<MusicBrainzMetadata> m_autoLookup;
    QString m_autoLookupDiscId;
    std::vector<QString> m_autoLookupReleaseIds;
    std::optional<TrackList> m_autoLookupTracks;
};
} // namespace Cdda
} // namespace Fooyin
