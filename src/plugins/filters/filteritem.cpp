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

#include "filteritem.h"

#include "constants.h"

#include <QVariant>

namespace Fy::Filters {
FilterItem::FilterItem(QString title, FilterItem* parent)
    : TreeItem{parent}
    , m_title(std::move(title))
{ }

void FilterItem::changeTitle(const QString& title)
{
    m_title = title;
}

QVariant FilterItem::data(int role) const
{
    switch(role) {
        case Constants::Role::Title:
            return m_title;
        case Constants::Role::Tracks:
            return QVariant::fromValue(m_tracks);
        default:
            return {};
    }
}

int FilterItem::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

void FilterItem::addTrack(Core::Track* track)
{
    m_tracks.emplace_back(track);
}

} // namespace Fy::Filters
