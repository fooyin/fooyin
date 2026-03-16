/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "database/trackdatabase.h"

#include <core/engine/audioinput.h>
#include <core/trackmetadatastore.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/worker.h>

#include <memory>
#include <stop_token>

namespace Fooyin {
class Database;
class AudioLoader;
class SettingsManager;
struct TrackCoverData;

class TrackDatabaseManager : public Worker
{
    Q_OBJECT

public:
    explicit TrackDatabaseManager(DbConnectionPoolPtr dbPool, std::shared_ptr<AudioLoader> audioLoader,
                                  SettingsManager* settings, std::shared_ptr<TrackMetadataStore> metadataStore = {},
                                  QObject* parent = nullptr);

    void initialiseThread() override;

signals:
    void gotTracks(const Fooyin::TrackList& tracks);
    void updatedTracks(const Fooyin::TrackList& tracks);
    void availabilityChecked(const Fooyin::TrackList& tracks);
    void updatedTracksStats(const Fooyin::TrackList& tracks);
    void removedTracks(const Fooyin::TrackList& tracks);
    void trackWriteCompleted(int operationId, const Fooyin::TrackList& tracks, int failed, bool cancelled);
    void trackCoverWriteCompleted(int operationId, const Fooyin::TrackList& tracks, int failed, bool cancelled);
    void unavailableTracksRemoved(int operationId, const Fooyin::TrackList& tracks, int failed, bool cancelled);

public slots:
    void getAllTracks();
    void checkTrackAvailability(const Fooyin::TrackList& tracks);
    void updateTracks(const Fooyin::TrackList& tracks, bool write);
    void updateTrackStats(const Fooyin::TrackList& track, AudioReader::WriteOptions writeOptions);
    void writeCovers(const Fooyin::TrackCoverData& tracks);
    void removeUnavailbleTracks(const TrackList& tracks);
    void cleanupTracks();

    void updateTracks(const Fooyin::TrackList& tracks, bool write, int operationId, std::stop_token stopToken);
    void writeCovers(const Fooyin::TrackCoverData& tracks, int operationId, std::stop_token stopToken);
    void removeUnavailbleTracks(const TrackList& tracks, int operationId, std::stop_token stopToken);

private:
    DbConnectionPoolPtr m_dbPool;
    std::shared_ptr<AudioLoader> m_audioLoader;
    SettingsManager* m_settings;
    std::shared_ptr<TrackMetadataStore> m_metadataStore;

    std::unique_ptr<DbConnectionHandler> m_dbHandler;
    TrackDatabase m_trackDatabase;
};
} // namespace Fooyin
