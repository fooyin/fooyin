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

#include "librarytreeitem.h"

namespace Fy::Gui::Widgets {
LibraryTreeItem::LibraryTreeItem()
    : LibraryTreeItem{"", nullptr, -1}
{ }

LibraryTreeItem::LibraryTreeItem(QString title, LibraryTreeItem* parent, int level)
    : TreeItem{parent}
    , m_pending{true}
    , m_level{level}
    , m_key{"0"}
    , m_title{std::move(title)}
{ }

bool LibraryTreeItem::pending() const
{
    return m_pending;
}

int LibraryTreeItem::level() const
{
    return m_level;
}

QString LibraryTreeItem::title() const
{
    return m_title;
}

Core::TrackList LibraryTreeItem::tracks() const
{
    return m_tracks;
}

int LibraryTreeItem::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

QString LibraryTreeItem::key() const
{
    return m_key;
}

void LibraryTreeItem::setPending(bool pending)
{
    m_pending = pending;
}

void LibraryTreeItem::setTitle(const QString& title)
{
    m_title = title;
}

void LibraryTreeItem::setKey(const QString& key)
{
    m_key = key;
}

void LibraryTreeItem::addTrack(const Core::Track& track)
{
    m_tracks.emplace_back(track);
}

void LibraryTreeItem::addTracks(const Core::TrackList& tracks)
{
    std::ranges::copy(tracks, std::back_inserter(m_tracks));
}

void LibraryTreeItem::sortChildren()
{
    std::vector<LibraryTreeItem*> sortedChildren{m_children};
    std::sort(sortedChildren.begin(), sortedChildren.end(), [](const LibraryTreeItem* lhs, const LibraryTreeItem* rhs) {
        if(lhs->m_level == -1) {
            return true;
        }
        if(rhs->m_level == -1) {
            return false;
        }
        const auto cmp = QString::localeAwareCompare(lhs->m_title, rhs->m_title);
        if(cmp == 0) {
            return false;
        }
        return cmp < 0;
    });
    m_children = sortedChildren;

    for(auto& child : m_children) {
        child->sortChildren();
    }
}
} // namespace Fy::Gui::Widgets
