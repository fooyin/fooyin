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

#pragma once

#include "playlistpreset.h"

#include <core/track.h>
#include <gui/scripting/scriptformatter.h>

#include <QSize>

namespace Fooyin {
class PlaylistScriptRegistry;

class PlaylistContainerItem
{
public:
    PlaylistContainerItem();

    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] int trackCount() const;

    [[nodiscard]] RichScript title() const;
    [[nodiscard]] RichScript subtitle() const;
    [[nodiscard]] RichScript sideText() const;
    [[nodiscard]] RichScript info() const;
    [[nodiscard]] int rowHeight() const;
    [[nodiscard]] QSize size() const;

    void updateGroupText(ScriptParser* parser, ScriptFormatter* formatter);

    void setTitle(const RichScript& title);
    void setSubtitle(const RichScript& subtitle);
    void setSideText(const RichScript& text);
    void setInfo(const RichScript& info);
    void setRowHeight(int height);

    void addTrack(const Track& track);
    void addTracks(const TrackList& tracks);
    void clearTracks();

    void calculateSize();

private:
    TrackList m_tracks;

    RichScript m_title;
    RichScript m_subtitle;
    RichScript m_sideText;
    RichScript m_info;

    QSize m_size;
    int m_rowHeight;
};

class PlaylistTrackItem
{
public:
    PlaylistTrackItem() = default;
    PlaylistTrackItem(std::vector<RichScript> columns, const Track& track);
    PlaylistTrackItem(RichScript left, RichScript right, const Track& track);

    [[nodiscard]] std::vector<RichScript> columns() const;
    [[nodiscard]] RichScript column(int column) const;
    [[nodiscard]] RichScript left() const;
    [[nodiscard]] RichScript right() const;
    [[nodiscard]] Track track() const;

private:
    std::vector<RichScript> m_columns;
    RichScript m_left;
    RichScript m_right;

    Track m_track;
};
} // namespace Fooyin
