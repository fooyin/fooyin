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

#include <QDataStream>
#include <QFont>
#include <QString>

#include <map>

namespace Fy::Gui::Widgets::Playlist {
struct PlaylistHeader
{
    QString title;
    QString subtitle;
    QString sideText;

    QFont titleFont;
    QFont subtitleFont;
    QFont sideTextFont;

    int rowHeight{73};
    bool showCover{true};
    bool simple{false};

    friend QDataStream& operator<<(QDataStream& stream, const PlaylistHeader& header)
    {
        stream << header.title;
        stream << header.titleFont.toString();
        stream << header.subtitle;
        stream << header.subtitleFont.toString();
        stream << header.sideText;
        stream << header.sideTextFont.toString();
        stream << header.rowHeight;
        stream << header.showCover;
        stream << header.simple;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, PlaylistHeader& header)
    {
        stream >> header.title;

        QString titleFont;
        stream >> titleFont;
        header.titleFont.fromString(titleFont);

        stream >> header.subtitle;

        QString subtitleFont;
        stream >> subtitleFont;
        header.subtitleFont.fromString(subtitleFont);

        stream >> header.sideText;

        QString sideTextFont;
        stream >> sideTextFont;
        header.sideTextFont.fromString(sideTextFont);

        stream >> header.rowHeight;
        stream >> header.showCover;
        stream >> header.simple;
        return stream;
    }
};

struct PlaylistSubHeader
{
    QString title;
    QFont titleFont;
    int rowHeight{25};

    friend QDataStream& operator<<(QDataStream& stream, const PlaylistSubHeader& header)
    {
        stream << header.title;
        stream << header.titleFont.toString();
        stream << header.rowHeight;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, PlaylistSubHeader& header)
    {
        stream >> header.title;
        QString titleFont;
        stream >> titleFont;
        header.titleFont.fromString(titleFont);
        stream >> header.rowHeight;
        return stream;
    }
};

struct PlaylistTrack
{
    QString leftText;
    QString rightText;

    QFont leftTextFont;
    QFont rightTextFont;

    int rowHeight{22};

    friend QDataStream& operator<<(QDataStream& stream, const PlaylistTrack& track)
    {
        stream << track.leftText;
        stream << track.leftTextFont.toString();
        stream << track.rightText;
        stream << track.rightTextFont.toString();
        stream << track.rowHeight;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, PlaylistTrack& track)
    {
        stream >> track.leftText;

        QString leftFont;
        stream >> leftFont;
        track.leftTextFont.fromString(leftFont);

        stream >> track.rightText;

        QString rightFont;
        stream >> rightFont;
        track.rightTextFont.fromString(rightFont);

        stream >> track.rowHeight;
        return stream;
    }
};

struct PlaylistPreset
{
    int index{-1};
    QString name;
    PlaylistHeader header;
    PlaylistSubHeader subHeader;
    PlaylistTrack track;

    bool isValid() const
    {
        return !name.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const PlaylistPreset& preset)
    {
        stream << preset.index;
        stream << preset.name;
        stream << preset.header;
        stream << preset.subHeader;
        stream << preset.track;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, PlaylistPreset& preset)
    {
        stream >> preset.index;
        stream >> preset.name;
        stream >> preset.header;
        stream >> preset.subHeader;
        stream >> preset.track;
        return stream;
    }
};

using IndexPresetMap = std::map<int, PlaylistPreset>;
} // namespace Fy::Gui::Widgets::Playlist
