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

#include <core/scripting/scriptparser.h>

#include <QColor>
#include <QDataStream>
#include <QFont>
#include <QString>

#include <QList>
#include <QVariant>

namespace Fy::Gui::Widgets::Playlist {
using Script = Core::Scripting::ParsedScript;

struct TextBlock
{
    QString text;
    Script script;

    bool fontChanged{false};
    QFont font;

    bool colourChanged{false};
    QColor colour;

    TextBlock();

    inline bool operator==(const TextBlock& other) const
    {
        return std::tie(text, fontChanged, font, colourChanged, colour)
            == std::tie(other.text, other.fontChanged, other.font, other.colourChanged, other.colour);
    };

    inline operator QVariant() const
    {
        return QVariant::fromValue(*this);
    }

    inline bool isValid()
    {
        return !text.isEmpty();
    }

    inline void cloneProperties(const TextBlock& other)
    {
        fontChanged   = other.fontChanged;
        font          = other.font;
        colourChanged = other.colourChanged;
        colour        = other.colour;
    }
};
using TextBlockList = QList<TextBlock>;

struct HeaderRow
{
    TextBlock title;
    TextBlock subtitle;
    TextBlock sideText;
    TextBlock info;

    int rowHeight{73};
    bool showCover{true};
    bool simple{false};

    inline bool operator==(const HeaderRow& other) const
    {
        return std::tie(title, subtitle, sideText, info, rowHeight, showCover, simple)
            == std::tie(other.title, other.subtitle, other.sideText, other.info, other.rowHeight, other.showCover,
                        other.simple);
    };

    inline bool isValid()
    {
        return title.isValid() || subtitle.isValid() || sideText.isValid();
    }
};

struct SubheaderRow
{
    TextBlock title;
    TextBlock info;

    inline bool operator==(const SubheaderRow& other) const
    {
        return std::tie(title, info) == std::tie(other.title, other.info);
    };

    inline bool isValid()
    {
        return title.isValid();
    }
};

struct SubheaderRows
{
    QList<SubheaderRow> rows;

    int rowHeight{25};

    inline bool operator==(const SubheaderRows& other) const
    {
        return std::tie(rows, rowHeight) == std::tie(other.rows, other.rowHeight);
    };

    inline bool isValid()
    {
        return !rows.isEmpty();
    }
};

struct TrackRow
{
    TextBlockList text;

    int rowHeight{22};

    inline bool operator==(const TrackRow& other) const
    {
        return std::tie(text, rowHeight) == std::tie(other.text, other.rowHeight);
    };

    inline bool isValid()
    {
        return !text.isEmpty();
    }
};

struct PlaylistPreset
{
    int index{-1};
    QString name;

    HeaderRow header;
    SubheaderRows subHeaders;
    TrackRow track;

    inline bool operator==(const PlaylistPreset& other) const
    {
        return std::tie(index, name, header, subHeaders, track)
            == std::tie(other.index, other.name, other.header, other.subHeaders, other.track);
    };

    inline bool isValid() const
    {
        return !name.isEmpty();
    };
};

QDataStream& operator<<(QDataStream& stream, const TextBlock& block);
QDataStream& operator>>(QDataStream& stream, TextBlock& block);
QDataStream& operator<<(QDataStream& stream, const HeaderRow& header);
QDataStream& operator>>(QDataStream& stream, HeaderRow& header);
QDataStream& operator<<(QDataStream& stream, const SubheaderRow& subheader);
QDataStream& operator>>(QDataStream& stream, SubheaderRow& subheader);
QDataStream& operator<<(QDataStream& stream, const SubheaderRows& subheaders);
QDataStream& operator>>(QDataStream& stream, SubheaderRows& subheaders);
QDataStream& operator<<(QDataStream& stream, const TrackRow& track);
QDataStream& operator>>(QDataStream& stream, TrackRow& track);
QDataStream& operator<<(QDataStream& stream, const PlaylistPreset& preset);
QDataStream& operator>>(QDataStream& stream, PlaylistPreset& preset);
} // namespace Fy::Gui::Widgets::Playlist

Q_DECLARE_METATYPE(Fy::Gui::Widgets::Playlist::TextBlock);
