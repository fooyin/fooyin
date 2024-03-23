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

#include <core/scripting/scriptparser.h>
#include <gui/scripting/scriptformatter.h>

#include <QColor>
#include <QDataStream>
#include <QFont>
#include <QString>

#include <QList>
#include <QVariant>

namespace Fooyin {
struct HeaderRow
{
    RichScript title;
    RichScript subtitle;
    RichScript sideText;
    RichScript info;

    int rowHeight{0};
    bool showCover{true};
    bool simple{false};

    bool operator==(const HeaderRow& other) const
    {
        return std::tie(title, subtitle, sideText, info, rowHeight, showCover, simple)
            == std::tie(other.title, other.subtitle, other.sideText, other.info, other.rowHeight, other.showCover,
                        other.simple);
    };

    [[nodiscard]] bool isValid() const
    {
        return !title.script.isEmpty() || !subtitle.script.isEmpty() || !sideText.script.isEmpty()
            || !info.script.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const HeaderRow& header);
    friend QDataStream& operator>>(QDataStream& stream, HeaderRow& header);
};

struct SubheaderRow
{
    RichScript leftText;
    RichScript rightText;

    int rowHeight{0};

    bool operator==(const SubheaderRow& other) const
    {
        return std::tie(leftText, rightText, rowHeight) == std::tie(other.leftText, other.rightText, other.rowHeight);
    };

    [[nodiscard]] bool isValid() const
    {
        return !leftText.script.isEmpty() || !rightText.script.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const SubheaderRow& subheader);
    friend QDataStream& operator>>(QDataStream& stream, SubheaderRow& subheader);
};
using SubheaderRows = QList<SubheaderRow>;

struct TrackRow
{
    std::vector<RichScript> columns;
    RichScript leftText;
    RichScript rightText;

    int rowHeight{0};

    bool operator==(const TrackRow& other) const
    {
        return std::tie(columns, leftText, rightText, rowHeight)
            == std::tie(other.columns, other.leftText, other.rightText, other.rowHeight);
    };

    [[nodiscard]] bool isValid() const
    {
        return !columns.empty() || !leftText.script.isEmpty() || !rightText.script.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const TrackRow& track);
    friend QDataStream& operator>>(QDataStream& stream, TrackRow& track);
};

struct PlaylistPreset
{
    int id{-1};
    int index{-1};
    bool isDefault{false};
    QString name;

    HeaderRow header;
    SubheaderRows subHeaders;
    TrackRow track;

    bool operator==(const PlaylistPreset& other) const
    {
        return std::tie(id, index, name, header, subHeaders, track)
            == std::tie(other.id, other.index, other.name, other.header, other.subHeaders, other.track);
    };

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty();
    };

    friend QDataStream& operator<<(QDataStream& stream, const PlaylistPreset& preset);
    friend QDataStream& operator>>(QDataStream& stream, PlaylistPreset& preset);
};
} // namespace Fooyin
