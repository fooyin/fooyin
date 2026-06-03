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

#include "infoitem.h"

#include <core/track.h>
#include <utils/worker.h>

#include <memory>

namespace Fooyin {
class InfoPopulatorPrivate;
class LibraryManager;

struct InfoData
{
    std::unordered_map<QString, std::vector<QString>> parents;
    std::unordered_map<QString, InfoItem> nodes;

    void clear()
    {
        parents.clear();
        nodes.clear();
    }
};

using InfoDataPtr = std::shared_ptr<InfoData>;

class InfoPopulator : public Worker
{
    Q_OBJECT

public:
    explicit InfoPopulator(LibraryManager* libraryManager, QObject* parent = nullptr);
    ~InfoPopulator() override;

    void run(InfoItem::Options options, const TrackList& tracks);

Q_SIGNALS:
    void populated(Fooyin::InfoDataPtr data);

private:
    std::unique_ptr<InfoPopulatorPrivate> p;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::InfoDataPtr)
