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

#include "playlistitemmodels.h"

#include "playlistscriptregistry.h"

namespace Fy::Gui::Widgets::Playlist {
Container::Container()
    : m_duration{0}
{ }

Core::TrackList Container::tracks() const
{
    return m_tracks;
}

int Container::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

uint64_t Container::duration() const
{
    return m_duration;
}

TextBlockList Container::title() const
{
    return m_title;
}

TextBlockList Container::subtitle() const
{
    return m_subtitle;
}

TextBlockList Container::sideText() const
{
    return m_sideText;
}

TextBlockList Container::info() const
{
    return m_info;
}

QString Container::genres() const
{
    return m_genres;
}

void Container::updateGroupText(Core::Scripting::Parser* parser, PlaylistScriptRegistry* registry)
{
    if(m_tracks.empty()) {
        return;
    }

    if(!parser || !registry) {
        return;
    }

    // Update duration
    m_duration = 0;
    for(const Core::Track& track : m_tracks) {
        m_duration += track.duration();
    }

    // Update genres
    QStringList genres;
    for(const auto& track : m_tracks) {
        for(const auto& genre : track.genres()) {
            if(!genres.contains(genre)) {
                genres.emplace_back(genre);
            }
        }
    }
    m_genres = genres.join(" / ");

    registry->changeCurrentContainer(this);

    const Core::Track& track = m_tracks.front();
    for(TextBlock& block : m_subtitle) {
        block.text = parser->evaluate(block.script, track);
    }
    for(TextBlock& block : m_info) {
        block.text = parser->evaluate(block.script, track);
    }
}

void Container::setTitle(const TextBlockList& title)
{
    m_title = title;
}

void Container::setSubtitle(const TextBlockList& subtitle)
{
    m_subtitle = subtitle;
}

void Container::setSideText(const TextBlockList& text)
{
    m_sideText = text;
}

void Container::setInfo(const TextBlockList& info)
{
    m_info = info;
}

void Container::addTrack(const Core::Track& track)
{
    m_tracks.emplace_back(track);
}

void Container::addTracks(const Core::TrackList& tracks)
{
    std::ranges::copy(tracks, std::back_inserter(m_tracks));
}

void Container::clearTracks()
{
    m_tracks.clear();
}

Track::Track(TextBlockList left, TextBlockList right, const Core::Track& track)
    : m_left{std::move(left)}
    , m_right{std::move(right)}
    , m_track{track}
{ }

TextBlockList Track::left() const
{
    return m_left;
}

TextBlockList Track::right() const
{
    return m_right;
}

Core::Track Track::track() const
{
    return m_track;
}
} // namespace Fy::Gui::Widgets::Playlist
