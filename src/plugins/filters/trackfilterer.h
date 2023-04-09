/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/models/trackfwd.h>

#include <utils/worker.h>

namespace Fy {
namespace Core::DB {
class Database;
}

namespace Filters {
class FilterDatabase;

class TrackFilterer : public Utils::Worker
{
    Q_OBJECT

public:
    explicit TrackFilterer(QObject* parent = nullptr);

    void filterTracks(const Core::TrackList& tracks, const QString& search);

signals:
    void tracksFiltered(const Core::TrackList& result);
};
} // namespace Filters
} // namespace Fy
