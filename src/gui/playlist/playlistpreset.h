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
    TextBlock(QString text, int fontSize);

    inline bool operator==(const TextBlock& other) const
    {
        return std::tie(text, fontChanged, font, colourChanged, colour)
            == std::tie(other.text, other.fontChanged, other.font, other.colourChanged, other.colour);
    };

    inline operator QVariant() const
    {
        return QVariant::fromValue(*this);
    }

    [[nodiscard]] inline bool isValid() const
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

class TextBlockList : public QList<TextBlock>
{
public:
    TextBlockList()
        : QList<TextBlock>{}
    { }

    explicit TextBlockList(const TextBlock& block)
    {
        this->append(block);
    }

    inline operator QVariant() const
    {
        return QVariant::fromValue(*this);
    }
};

struct HeaderRow
{
    TextBlockList title;
    TextBlockList subtitle;
    TextBlockList sideText;
    TextBlockList info;

    int rowHeight{73};
    bool showCover{true};
    bool simple{false};

    inline bool operator==(const HeaderRow& other) const
    {
        return std::tie(title, subtitle, sideText, info, rowHeight, showCover, simple)
            == std::tie(other.title, other.subtitle, other.sideText, other.info, other.rowHeight, other.showCover,
                        other.simple);
    };

    [[nodiscard]] inline bool isValid() const
    {
        return !title.empty() || !subtitle.empty() || !sideText.empty();
    }
};

struct SubheaderRow
{
    TextBlockList leftText;
    TextBlockList rightText;

    int rowHeight{22};

    inline bool operator==(const SubheaderRow& other) const
    {
        return std::tie(leftText, rightText, rowHeight) == std::tie(other.leftText, other.rightText, other.rowHeight);
    };

    [[nodiscard]] inline bool isValid() const
    {
        return !leftText.isEmpty();
    }
};
using SubheaderRows = QList<SubheaderRow>;

struct TrackRow
{
    TextBlockList leftText;
    TextBlockList rightText;

    int rowHeight{22};

    inline bool operator==(const TrackRow& other) const
    {
        return std::tie(leftText, rightText, rowHeight) == std::tie(other.leftText, other.rightText, other.rowHeight);
    };

    [[nodiscard]] inline bool isValid() const
    {
        return !leftText.isEmpty() || !rightText.isEmpty();
    }
};

struct PlaylistPreset
{
    int id{-1};
    int index{-1};
    QString name;

    HeaderRow header;
    SubheaderRows subHeaders;
    TrackRow track;

    inline bool operator==(const PlaylistPreset& other) const
    {
        return std::tie(id, index, name, header, subHeaders, track)
            == std::tie(other.id, other.index, other.name, other.header, other.subHeaders, other.track);
    };

    [[nodiscard]] inline bool isValid() const
    {
        return id >= 0 && !name.isEmpty();
    };
};

QDataStream& operator<<(QDataStream& stream, const TextBlock& block);
QDataStream& operator>>(QDataStream& stream, TextBlock& block);
QDataStream& operator<<(QDataStream& stream, const HeaderRow& header);
QDataStream& operator>>(QDataStream& stream, HeaderRow& header);
QDataStream& operator<<(QDataStream& stream, const SubheaderRow& subheader);
QDataStream& operator>>(QDataStream& stream, SubheaderRow& subheader);
QDataStream& operator<<(QDataStream& stream, const TrackRow& track);
QDataStream& operator>>(QDataStream& stream, TrackRow& track);
QDataStream& operator<<(QDataStream& stream, const PlaylistPreset& preset);
QDataStream& operator>>(QDataStream& stream, PlaylistPreset& preset);
} // namespace Fy::Gui::Widgets::Playlist

Q_DECLARE_METATYPE(Fy::Gui::Widgets::Playlist::TextBlock);
Q_DECLARE_METATYPE(Fy::Gui::Widgets::Playlist::TextBlockList);
