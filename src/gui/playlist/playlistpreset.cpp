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

#include "playlistpreset.h"

constexpr auto PlaylistPresetVersionMarker = -1;
constexpr auto PlaylistPresetVersion       = 5;

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString combinedScript(const QString& left, const QString& right, bool addLine = false)
{
    QString script;
    if(!left.isEmpty()) {
        script = u"<left>"_s + left + u"</left>"_s;
    }
    if(addLine) {
        script += u"\n<hr/>\n"_s;
    }
    if(!right.isEmpty()) {
        script += u"<right>"_s + right + u"</right>"_s;
    }
    return script;
}
} // namespace

QDataStream& operator<<(QDataStream& stream, const HeaderRow& header)
{
    stream << header.text;
    stream << header.rowHeight;
    stream << header.showCover;
    stream << header.artworkPadding;
    stream << header.artworkPaddingVertical;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, HeaderRow& header)
{
    stream >> header.text;
    stream >> header.rowHeight;
    stream >> header.showCover;
    stream >> header.artworkPadding;
    stream >> header.artworkPaddingVertical;
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const SubheaderRow& subheader)
{
    stream << subheader.text;
    stream << subheader.rowHeight;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, SubheaderRow& subheader)
{
    stream >> subheader.text;
    stream >> subheader.rowHeight;
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const TrackRow& track)
{
    stream << track.text;
    stream << track.rowHeight;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, TrackRow& track)
{
    stream >> track.text;
    stream >> track.rowHeight;
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const PlaylistPreset& preset)
{
    stream << PlaylistPresetVersionMarker;
    stream << PlaylistPresetVersion;
    stream << preset.id;
    stream << preset.index;
    stream << preset.name;
    stream << preset.header;
    stream << preset.subHeaders;
    stream << preset.track;
    stream << preset.insetSubheadersToImageColumns;
    stream << preset.showCoverBelowEverySubheader;
    stream << preset.header.grouping;

    QStringList subheaderGroupings;
    subheaderGroupings.reserve(preset.subHeaders.size());
    for(const auto& subheader : preset.subHeaders) {
        subheaderGroupings.push_back(subheader.grouping);
    }
    stream << subheaderGroupings;

    return stream;
}

QDataStream& operator>>(QDataStream& stream, PlaylistPreset& preset)
{
    int version{1};

    stream >> preset.id;
    if(preset.id == PlaylistPresetVersionMarker) {
        stream >> version;
        stream >> preset.id;
    }

    stream >> preset.index;
    stream >> preset.name;

    if(version >= 5) {
        stream >> preset.header;
    }
    else {
        RichScript title;
        RichScript subtitle;
        RichScript side;
        RichScript info;
        bool simple{false};

        stream >> title >> subtitle >> side >> info;
        stream >> preset.header.rowHeight >> preset.header.showCover >> simple;

        auto& script = preset.header.text.script;

        if(simple) {
            preset.header.showCover = false;
            script                  = combinedScript(title.script, side.script, !title.script.isEmpty());
        }
        else {
            std::vector<QString> lines;

            if(!title.script.isEmpty()) {
                lines.push_back(u"<left>"_s + title.script + u"</left>"_s);
            }
            if(!subtitle.script.isEmpty() || !side.script.isEmpty()) {
                const bool addLine = !subtitle.script.isEmpty() && !side.script.isEmpty();
                lines.push_back(combinedScript(subtitle.script, side.script, addLine));
            }
            if(!info.script.isEmpty()) {
                lines.push_back(u"<left>"_s + info.script + u"</left>"_s);
            }
            if(!lines.empty()) {
                lines.push_back(u"<hr/>"_s);
            }

            for(const auto& line : lines) {
                if(!script.isEmpty()) {
                    script += u"\n$crlf()\n"_s;
                }
                script += line;
            }
        }
    }

    if(version >= 5) {
        stream >> preset.subHeaders;
        stream >> preset.track;
    }
    else {
        quint32 subheaderCount{0};
        stream >> subheaderCount;

        for(quint32 i{0}; i < subheaderCount && stream.status() == QDataStream::Ok; ++i) {
            SubheaderRow subheader;
            RichScript left;
            RichScript right;
            stream >> left >> right >> subheader.rowHeight;
            subheader.text.script = combinedScript(left.script, right.script, true);
            preset.subHeaders.push_back(std::move(subheader));
        }

        RichScript left;
        RichScript right;
        stream >> left >> right >> preset.track.rowHeight;
        preset.track.text.script = combinedScript(left.script, right.script);
    }

    if(version >= 2 && !stream.atEnd()) {
        stream >> preset.insetSubheadersToImageColumns;
    }
    if(version >= 3 && !stream.atEnd()) {
        stream >> preset.showCoverBelowEverySubheader;
    }
    if(version >= 4 && !stream.atEnd()) {
        stream >> preset.header.grouping;

        QStringList subheaderGroupings;
        stream >> subheaderGroupings;
        const auto count = std::min(preset.subHeaders.size(), subheaderGroupings.size());
        for(qsizetype i{0}; i < count; ++i) {
            preset.subHeaders[i].grouping = subheaderGroupings.at(i);
        }
    }

    return stream;
}
} // namespace Fooyin
