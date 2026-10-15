/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
    QString grouping;
    RichScript text;

    int rowHeight{0};
    bool showCover{true};
    int artworkPadding{10};
    int artworkPaddingVertical{10};

    bool operator==(const HeaderRow& other) const = default;

    [[nodiscard]] bool isValid() const
    {
        return !text.script.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const HeaderRow& header);
    friend QDataStream& operator>>(QDataStream& stream, HeaderRow& header);
};

struct SubheaderRow
{
    QString grouping;
    RichScript text;

    int rowHeight{0};

    bool operator==(const SubheaderRow& other) const = default;

    [[nodiscard]] bool isValid() const
    {
        return !text.script.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const SubheaderRow& subheader);
    friend QDataStream& operator>>(QDataStream& stream, SubheaderRow& subheader);
};
using SubheaderRows = QList<SubheaderRow>;

struct TrackRow
{
    std::vector<RichScript> columns;
    RichScript text;

    int rowHeight{0};

    bool operator==(const TrackRow& other) const = default;

    [[nodiscard]] bool isValid() const
    {
        return !columns.empty() || !text.script.isEmpty();
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
    bool insetSubheadersToImageColumns{false};
    bool showCoverBelowEverySubheader{false};

    bool operator==(const PlaylistPreset& other) const
    {
        return std::tie(id, index, name, header, subHeaders, track, insetSubheadersToImageColumns,
                        showCoverBelowEverySubheader)
            == std::tie(other.id, other.index, other.name, other.header, other.subHeaders, other.track,
                        other.insetSubheadersToImageColumns, other.showCoverBelowEverySubheader);
    };

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty();
    };

    friend QDataStream& operator<<(QDataStream& stream, const PlaylistPreset& preset);
    friend QDataStream& operator>>(QDataStream& stream, PlaylistPreset& preset);
};
} // namespace Fooyin
