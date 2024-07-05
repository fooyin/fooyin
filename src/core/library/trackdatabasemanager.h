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

#include <core/tagging/tagparser.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/worker.h>

namespace Fooyin {
class Database;
class TagLoader;

class TrackDatabaseManager : public Worker
{
    Q_OBJECT

public:
    explicit TrackDatabaseManager(DbConnectionPoolPtr dbPool, std::shared_ptr<TagLoader> tagLoader,
                                  QObject* parent = nullptr);

    void initialiseThread() override;

signals:
    void gotTracks(const Fooyin::TrackList& tracks);
    void updatedTracks(const Fooyin::TrackList& tracks);

public slots:
    void getAllTracks();
    void updateTracks(const Fooyin::TrackList& tracks);
    void updateTrackStats(const Fooyin::TrackList& track);
    void cleanupTracks();

private:
    DbConnectionPoolPtr m_dbPool;
    std::shared_ptr<TagLoader> m_tagLoader;
    std::unique_ptr<DbConnectionHandler> m_dbHandler;
    TrackDatabase m_trackDatabase;
};
} // namespace Fooyin
