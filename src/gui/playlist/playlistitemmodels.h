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

#include "playlistpreset.h"

#include <core/track.h>

namespace Fooyin {
class PlaylistScriptRegistry;

class PlaylistContainerItem
{
public:
    PlaylistContainerItem();

    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] uint64_t duration() const;

    [[nodiscard]] TextBlockList title() const;
    [[nodiscard]] TextBlockList subtitle() const;
    [[nodiscard]] TextBlockList sideText() const;
    [[nodiscard]] TextBlockList info() const;
    [[nodiscard]] int rowHeight() const;

    [[nodiscard]] QString genres() const;
    [[nodiscard]] QString filetypes() const;

    void updateGroupText(ScriptParser* parser, PlaylistScriptRegistry* registry);

    void setTitle(const TextBlockList& title);
    void setSubtitle(const TextBlockList& subtitle);
    void setSideText(const TextBlockList& text);
    void setInfo(const TextBlockList& info);
    void setRowHeight(int height);

    void addTrack(const Track& track);
    void addTracks(const TrackList& tracks);
    void clearTracks();

private:
    TrackList m_tracks;
    uint64_t m_duration;
    QString m_genres;
    QString m_filetypes;

    TextBlockList m_title;
    TextBlockList m_subtitle;
    TextBlockList m_sideText;
    TextBlockList m_info;

    int m_rowHeight;
};

class PlaylistTrackItem
{
public:
    PlaylistTrackItem() = default;
    PlaylistTrackItem(TextBlockList left, TextBlockList right, const Track& track);

    [[nodiscard]] TextBlockList left() const;
    [[nodiscard]] TextBlockList right() const;
    [[nodiscard]] Track track() const;

private:
    TextBlockList m_left;
    TextBlockList m_right;

    Track m_track;
};
} // namespace Fooyin
