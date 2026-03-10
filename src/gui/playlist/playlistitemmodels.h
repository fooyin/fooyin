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

#include <core/playlist/playlist.h>
#include <core/track.h>
#include <gui/scripting/scriptformatter.h>

#include <QSize>

#include <optional>

namespace Fooyin {
class ScriptParser;

class PlaylistContainerItem
{
public:
    explicit PlaylistContainerItem(bool isSimple);

    [[nodiscard]] const RichText& title() const;
    [[nodiscard]] const RichText& subtitle() const;
    [[nodiscard]] const RichText& sideText() const;
    [[nodiscard]] const RichText& info() const;
    [[nodiscard]] int rowHeight() const;
    [[nodiscard]] QSize size() const;
    [[nodiscard]] int scriptIndex() const;
    [[nodiscard]] const std::optional<Track>& coverTrack() const;

    void setTitle(const RichText& title);
    void setSubtitle(const RichText& subtitle);
    void setSideText(const RichText& text);
    void setInfo(const RichText& info);
    void setRowHeight(int height);
    void setScriptIndex(int index);
    void setCoverTrack(const Track& track);
    void clearCoverTrack();

    void calculateSize();

private:
    RichText m_title;
    RichText m_subtitle;
    RichText m_sideText;
    RichText m_info;

    bool m_simple;
    QSize m_size;
    int m_rowHeight;
    int m_scriptIndex{-1};
    std::optional<Track> m_coverTrack;
};

class PlaylistTrackItem
{
public:
    PlaylistTrackItem() = default;
    PlaylistTrackItem(std::vector<RichText> columns, const PlaylistTrack& track);
    PlaylistTrackItem(RichText left, RichText right, const PlaylistTrack& track);

    [[nodiscard]] const std::vector<RichText>& columns() const;
    [[nodiscard]] const RichText& column(int column) const;
    [[nodiscard]] const RichText& left() const;
    [[nodiscard]] const RichText& right() const;
    [[nodiscard]] const PlaylistTrack& track() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] int rowHeight() const;
    [[nodiscard]] int depth() const;
    [[nodiscard]] QSize size(int column = 0) const;

    void setColumns(const std::vector<RichText>& columns);
    void setLeftRight(const RichText& left, const RichText& right);
    void setTrack(const PlaylistTrack& track);
    void setIndex(int index);

    void setRowHeight(int height);
    void setDepth(int depth);
    void removeColumn(int column);

    void calculateSize();

private:
    std::vector<RichText> m_columns;
    RichText m_left;
    RichText m_right;

    PlaylistTrack m_track;
    mutable std::vector<QSize> m_sizes;
    int m_rowHeight;
    int m_depth;
};
} // namespace Fooyin
