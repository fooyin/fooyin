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

#include "playlistpreset.h"

#include <QApplication>
#include <QPalette>

namespace Fooyin {
TextBlock::TextBlock()
    : TextBlock{QStringLiteral("")}
{ }

TextBlock::TextBlock(QString script_, int fontDelta)
    : script{std::move(script_)}
    , colour{QApplication::palette().text().color()}
{
    font.setPointSize(font.pointSize() + fontDelta);
}

QDataStream& operator<<(QDataStream& stream, const TextBlock& block)
{
    stream << block.script;
    stream << block.fontChanged;
    if(block.fontChanged) {
        stream << block.font;
    }
    stream << block.colourChanged;
    if(block.colourChanged) {
        stream << block.colour;
    }
    return stream;
}

QDataStream& operator>>(QDataStream& stream, TextBlock& block)
{
    stream >> block.script;
    stream >> block.fontChanged;
    if(block.fontChanged) {
        stream >> block.font;
    }
    stream >> block.colourChanged;
    if(block.colourChanged) {
        stream >> block.colour;
    }
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const TextBlockList& blocks)
{
    stream << static_cast<int>(blocks.size());

    for(const auto& block : blocks) {
        stream << block;
    }

    return stream;
}

QDataStream& operator>>(QDataStream& stream, TextBlockList& blocks)
{
    int size;
    stream >> size;

    while(size > 0) {
        --size;

        TextBlock block;
        stream >> block;

        blocks.push_back(block);
    }

    return stream;
}

QDataStream& operator<<(QDataStream& stream, const HeaderRow& header)
{
    stream << header.title;
    stream << header.subtitle;
    stream << header.sideText;
    stream << header.info;
    stream << header.rowHeight;
    stream << header.showCover;
    stream << header.simple;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, HeaderRow& header)
{
    stream >> header.title;
    stream >> header.subtitle;
    stream >> header.sideText;
    stream >> header.info;
    stream >> header.rowHeight;
    stream >> header.showCover;
    stream >> header.simple;
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const SubheaderRow& subheader)
{
    stream << subheader.leftText;
    stream << subheader.rightText;
    stream << subheader.rowHeight;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, SubheaderRow& subheader)
{
    stream >> subheader.leftText;
    stream >> subheader.rightText;
    stream >> subheader.rowHeight;
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const TrackRow& track)
{
    stream << track.leftText;
    stream << track.rightText;
    stream << track.rowHeight;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, TrackRow& track)
{
    stream >> track.leftText;
    stream >> track.rightText;
    stream >> track.rowHeight;
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const PlaylistPreset& preset)
{
    stream << preset.id;
    stream << preset.index;
    stream << preset.name;
    stream << preset.header;
    stream << preset.subHeaders;
    stream << preset.track;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, PlaylistPreset& preset)
{
    stream >> preset.id;
    stream >> preset.index;
    stream >> preset.name;
    stream >> preset.header;
    stream >> preset.subHeaders;
    stream >> preset.track;
    return stream;
}
} // namespace Fooyin
