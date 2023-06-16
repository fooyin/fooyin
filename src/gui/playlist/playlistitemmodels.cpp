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

#include <ranges>

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

TextBlock Container::title() const
{
    return m_title;
}

TextBlock Container::subtitle() const
{
    return m_subtitle;
}

TextBlock Container::sideText() const
{
    return m_sideText;
}

TextBlock Container::info() const
{
    return m_info;
}

bool Container::hasCover() const
{
    return !m_coverPath.isEmpty();
}

QString Container::coverPath() const
{
    return m_coverPath;
}

QString Container::genres() const
{
    if(!m_genres.isEmpty()) {
        return m_genres;
    }

    QStringList genres;
    for(const auto& track : m_tracks) {
        for(const auto& genre : track.genres()) {
            if(!genres.contains(genre)) {
                genres.emplace_back(genre);
            }
        }
    }
    m_genres = genres.join(" / ");
    return m_genres;
}

void Container::setTitle(const TextBlock& title)
{
    m_title = title;
}

void Container::setSubtitle(const TextBlock& subtitle)
{
    m_subtitle = subtitle;
}

void Container::setSideText(const TextBlock& text)
{
    m_sideText = text;
}

void Container::setCoverPath(const QString& path)
{
    m_coverPath = path;
}

void Container::addTrack(const Core::Track& track)
{
    m_tracks.emplace_back(track);
    m_duration += track.duration();
}

void Container::removeTrack(const Core::Track& trackToRemove)
{
    m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(),
                                  [trackToRemove](const Core::Track& track) {
                                      return track == trackToRemove;
                                  }),
                   m_tracks.end());

    m_duration -= trackToRemove.duration();
}

void Container::modifyInfo(TextBlock info)
{
    m_info = std::move(info);
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