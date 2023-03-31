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

#include <utils/treeitem.h>

#include <QObject>

namespace Fy::Filters {
class FilterItem : public Utils::TreeItem<FilterItem>
{
public:
    explicit FilterItem(QString title = "", FilterItem* parent = {});

    void changeTitle(const QString& title);

    [[nodiscard]] QVariant data(int role = 0) const;
    [[nodiscard]] int trackCount() const;
    void addTrack(Core::Track* track);

private:
    QString m_title;
    Core::TrackPtrList m_tracks;
};
} // namespace Fy::Filters
