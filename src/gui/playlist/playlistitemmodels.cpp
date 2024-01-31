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

#include "playlistitemmodels.h"

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
PlaylistContainerItem::PlaylistContainerItem()
    : m_rowHeight{-1}
{ }

TrackList PlaylistContainerItem::tracks() const
{
    return m_tracks;
}

int PlaylistContainerItem::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

TextBlockList PlaylistContainerItem::title() const
{
    return m_title;
}

TextBlockList PlaylistContainerItem::subtitle() const
{
    return m_subtitle;
}

TextBlockList PlaylistContainerItem::sideText() const
{
    return m_sideText;
}

TextBlockList PlaylistContainerItem::info() const
{
    return m_info;
}

int PlaylistContainerItem::rowHeight() const
{
    return m_rowHeight;
}

void PlaylistContainerItem::updateGroupText(ScriptParser* parser)
{
    if(m_tracks.empty()) {
        return;
    }

    if(!parser) {
        return;
    }

    auto evaluateBlocks = [this, parser](TextBlockList& blocks) {
        std::erase_if(blocks, [parser, this](TextBlock& block) {
            block.text = parser->evaluate(block.script, m_tracks);
            return block.text.isNull();
        });
    };

    evaluateBlocks(m_title);
    evaluateBlocks(m_subtitle);
    evaluateBlocks(m_info);
    evaluateBlocks(m_sideText);
}

void PlaylistContainerItem::setTitle(const TextBlockList& title)
{
    m_title = title;
}

void PlaylistContainerItem::setSubtitle(const TextBlockList& subtitle)
{
    m_subtitle = subtitle;
}

void PlaylistContainerItem::setSideText(const TextBlockList& text)
{
    m_sideText = text;
}

void PlaylistContainerItem::setInfo(const TextBlockList& info)
{
    m_info = info;
}

void PlaylistContainerItem::setRowHeight(int height)
{
    m_rowHeight = height;
}

void PlaylistContainerItem::addTrack(const Track& track)
{
    m_tracks.emplace_back(track);
}

void PlaylistContainerItem::addTracks(const TrackList& tracks)
{
    std::ranges::copy(tracks, std::back_inserter(m_tracks));
}

void PlaylistContainerItem::clearTracks()
{
    m_tracks.clear();
}

PlaylistTrackItem::PlaylistTrackItem(TextBlockList left, TextBlockList right, const Track& track)
    : m_left{std::move(left)}
    , m_right{std::move(right)}
    , m_track{track}
{ }

TextBlockList PlaylistTrackItem::left() const
{
    return m_left;
}

TextBlockList PlaylistTrackItem::right() const
{
    return m_right;
}

Track PlaylistTrackItem::track() const
{
    return m_track;
}
} // namespace Fooyin
