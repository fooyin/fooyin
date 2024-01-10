/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
#include "tagging/tagwriter.h"

#include <utils/worker.h>

namespace Fooyin {
class Database;

class TrackDatabaseManager : public Worker
{
    Q_OBJECT

public:
    explicit TrackDatabaseManager(Database* database, QObject* parent = nullptr);

    void closeThread() override;

signals:
    void gotTracks(const TrackList& tracks);
    void updatedTracks(const TrackList& tracks);

public slots:
    void getAllTracks();
    void updateTracks(const TrackList& tracks);
    void updateTrackStats(const Track& track);
    void cleanupTracks();

private:
    Database* m_database;
    TrackDatabase m_trackDatabase;
    TagWriter m_tagWriter;
};
} // namespace Fooyin
