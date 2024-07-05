/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
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

class InfoPopulator : public Worker
{
    Q_OBJECT

public:
    explicit InfoPopulator(QObject* parent = nullptr);
    ~InfoPopulator() override;

    void run(InfoItem::Options options, const TrackList& tracks);

signals:
    void populated(Fooyin::InfoData data);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
