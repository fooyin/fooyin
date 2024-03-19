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

    [[nodiscard]] FormattedScript title() const;
    [[nodiscard]] FormattedScript subtitle() const;
    [[nodiscard]] FormattedScript sideText() const;
    [[nodiscard]] FormattedScript info() const;
    [[nodiscard]] int rowHeight() const;
    [[nodiscard]] QSize size() const;

    void updateGroupText(ScriptParser* parser, ScriptFormatter* formatter);

    void setTitle(const FormattedScript& title);
    void setSubtitle(const FormattedScript& subtitle);
    void setSideText(const FormattedScript& text);
    void setInfo(const FormattedScript& info);
    void setRowHeight(int height);

    void addTrack(const Track& track);
    void addTracks(const TrackList& tracks);
    void clearTracks();

    void calculateSize();

private:
    TrackList m_tracks;

    FormattedScript m_title;
    FormattedScript m_subtitle;
    FormattedScript m_sideText;
    FormattedScript m_info;

    QSize m_size;
    int m_rowHeight;
};

class PlaylistTrackItem
{
public:
    PlaylistTrackItem() = default;
    PlaylistTrackItem(std::vector<FormattedScript> columns, const Track& track);
    PlaylistTrackItem(FormattedScript left, FormattedScript right, const Track& track);

    [[nodiscard]] std::vector<FormattedScript> columns() const;
    [[nodiscard]] FormattedScript column(int column) const;
    [[nodiscard]] FormattedScript left() const;
    [[nodiscard]] FormattedScript right() const;
    [[nodiscard]] Track track() const;

private:
    std::vector<FormattedScript> m_columns;
    FormattedScript m_left;
    FormattedScript m_right;

    Track m_track;
};
} // namespace Fooyin
