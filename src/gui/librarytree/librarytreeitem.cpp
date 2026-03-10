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

#include "librarytreeitem.h"

#include <core/constants.h>
#include <core/library/tracksort.h>

namespace {
Fooyin::RichText plainTextToRichText(const QString& text)
{
    Fooyin::RichText richText;
    if(text.isEmpty()) {
        return richText;
    }

    richText.blocks.push_back({.text = text, .format = {}});
    return richText;
}

void removeCoverMarker(Fooyin::RichText& richText, const char* cover)
{
    for(auto blockIt = richText.blocks.begin(); blockIt != richText.blocks.end();) {
        blockIt->text.remove(QLatin1String{cover});

        if(blockIt->text.isEmpty()) {
            blockIt = richText.blocks.erase(blockIt);
        }
        else {
            ++blockIt;
        }
    }
}

QStyleOptionViewItem::Position getCoverPosition(const QString& text, const char* cover)
{
    if(text.indexOf(QLatin1String{cover}) == 0) {
        return QStyleOptionViewItem::Left;
    }

    return QStyleOptionViewItem::Right;
}
} // namespace

namespace Fooyin {
LibraryTreeItem::LibraryTreeItem()
    : LibraryTreeItem{{}, nullptr, -1}
{ }

LibraryTreeItem::LibraryTreeItem(QString title, LibraryTreeItem* parent, int level)
    : TreeItem{parent}
    , m_pending{false}
    , m_level{level}
    , m_title{std::move(title)}
    , m_titleSource{m_title}
    , m_richTitle{plainTextToRichText(m_title)}
    , m_coverPosition{QStyleOptionViewItem::Left}
{
    if(m_title.contains(QLatin1String{Constants::FrontCover})) {
        m_coverType     = Track::Cover::Front;
        m_coverPosition = getCoverPosition(m_title, Constants::FrontCover);
        m_title.remove(QLatin1String{Constants::FrontCover});
        removeCoverMarker(m_richTitle, Constants::FrontCover);
    }
    else if(m_title.contains(QLatin1String{Constants::BackCover})) {
        m_coverType     = Track::Cover::Back;
        m_coverPosition = getCoverPosition(m_title, Constants::BackCover);
        m_title.remove(QLatin1String{Constants::BackCover});
        removeCoverMarker(m_richTitle, Constants::BackCover);
    }
    else if(m_title.contains(QLatin1String{Constants::ArtistPicture})) {
        m_coverType     = Track::Cover::Artist;
        m_coverPosition = getCoverPosition(m_title, Constants::ArtistPicture);
        m_title.remove(QLatin1String{Constants::ArtistPicture});
        removeCoverMarker(m_richTitle, Constants::ArtistPicture);
    }
}

bool LibraryTreeItem::pending() const
{
    return m_pending;
}

int LibraryTreeItem::level() const
{
    return m_level;
}

const QString& LibraryTreeItem::title() const
{
    return m_title;
}

const QString& LibraryTreeItem::titleSource() const
{
    return m_titleSource;
}

const RichText& LibraryTreeItem::richTitle() const
{
    return m_richTitle;
}

const TrackList& LibraryTreeItem::tracks() const
{
    return m_tracks;
}

int LibraryTreeItem::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

const Md5Hash& LibraryTreeItem::key() const
{
    return m_key;
}

std::optional<Track::Cover> LibraryTreeItem::coverType() const
{
    return m_coverType;
}

QStyleOptionViewItem::Position LibraryTreeItem::coverPosition() const
{
    return m_coverPosition;
}

void LibraryTreeItem::setPending(bool pending)
{
    m_pending = pending;
}

void LibraryTreeItem::setTitle(const QString& title)
{
    m_title       = title;
    m_titleSource = m_title;
    m_richTitle   = plainTextToRichText(m_title);
}

void LibraryTreeItem::setTitleSource(const QString& title)
{
    m_titleSource = title;
}

void LibraryTreeItem::setRichTitle(const RichText& title)
{
    m_richTitle = title;

    if(m_coverType == Track::Cover::Front) {
        removeCoverMarker(m_richTitle, Constants::FrontCover);
    }
    else if(m_coverType == Track::Cover::Back) {
        removeCoverMarker(m_richTitle, Constants::BackCover);
    }
    else if(m_coverType == Track::Cover::Artist) {
        removeCoverMarker(m_richTitle, Constants::ArtistPicture);
    }
}

void LibraryTreeItem::setKey(const Md5Hash& key)
{
    m_key = key;
}

void LibraryTreeItem::addTrack(const Track& track)
{
    m_tracks.emplace_back(track);
}

void LibraryTreeItem::addTracks(const TrackList& tracks)
{
    m_tracks.reserve(m_tracks.size() + tracks.size());
    std::ranges::copy(tracks, std::back_inserter(m_tracks));
}

void LibraryTreeItem::removeTrack(const Track& track)
{
    if(m_tracks.empty()) {
        return;
    }
    std::erase_if(m_tracks, [track](const Track& child) { return child.id() == track.id(); });
}

void LibraryTreeItem::replaceTrack(const Track& track)
{
    if(m_tracks.empty()) {
        return;
    }
    std::ranges::replace_if(m_tracks, [track](const Track& child) { return child.id() == track.id(); }, track);
}

void LibraryTreeItem::sortTracks(TrackSorter& sorter, const QString& script)
{
    if(m_tracks.empty() || script.isEmpty()) {
        return;
    }

    m_tracks = sorter.calcSortTracks(script, m_tracks);
}
} // namespace Fooyin
