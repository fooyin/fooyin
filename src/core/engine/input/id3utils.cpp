/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "id3utils.h"

#include <core/engine/tagdefs.h>

#include <QStringDecoder>
#include <QtEndian>

#include <algorithm>
#include <cstdint>

using namespace Qt::StringLiterals;

namespace Fooyin::Id3Utils {
namespace {
enum class Id3Version : uchar
{
    V2_3 = 3,
    V2_4 = 4,
};

enum class Id3TextEncoding : uchar
{
    Latin1  = 0,
    Utf16   = 1,
    Utf16BE = 2,
    Utf8    = 3,
};

uint32_t bigEndianUint32(QByteArrayView data, qsizetype offset)
{
    return qFromBigEndian<uint32_t>(data.data() + offset);
}

uint32_t syncSafeUint32(QByteArrayView data, qsizetype offset)
{
    return (static_cast<uint32_t>(static_cast<uchar>(data.at(offset))) << 21U)
         | (static_cast<uint32_t>(static_cast<uchar>(data.at(offset + 1))) << 14U)
         | (static_cast<uint32_t>(static_cast<uchar>(data.at(offset + 2))) << 7U)
         | static_cast<uint32_t>(static_cast<uchar>(data.at(offset + 3)));
}

QString decodeId3Text(QByteArrayView data)
{
    if(data.isEmpty()) {
        return {};
    }

    const auto encoding = static_cast<Id3TextEncoding>(data.front());
    data                = data.sliced(1);

    QString text;
    switch(encoding) {
        case Id3TextEncoding::Latin1:
            text = QString::fromLatin1(data);
            break;
        case Id3TextEncoding::Utf16:
            text = QStringDecoder{QStringDecoder::Utf16}(data);
            break;
        case Id3TextEncoding::Utf16BE:
            text = QStringDecoder{QStringDecoder::Utf16BE}(data);
            break;
        case Id3TextEncoding::Utf8:
            text = QString::fromUtf8(data);
            break;
        default:
            return {};
    }

    text.remove(QChar::Null);
    return text.trimmed();
}

QStringList splitValues(const QStringList& values, QChar separator)
{
    QStringList result;
    for(const QString& value : values) {
        const QStringList parts = value.split(separator, Qt::SkipEmptyParts);
        for(const QString& part : parts) {
            result.append(part.trimmed());
        }
    }
    return result;
}

QStringList splitSlashSeparatedValues(const QStringList& values)
{
    QStringList result;
    for(const QString& value : values) {
        const QStringList parts = value.split(" / "_L1, Qt::SkipEmptyParts);
        for(const QString& part : parts) {
            result.append(part.trimmed());
        }
    }
    return result;
}

bool isSlashSeparatedField(const QString& field)
{
    return field == QLatin1StringView{Tag::Artist} || field == QLatin1StringView{Tag::Composer}
        || field == QLatin1StringView{Tag::Performer};
}

bool isSlashSeparatedExtraField(const QString& field)
{
    return field == "LYRICIST"_L1 || field == "ORIGINALLYRICIST"_L1 || field == "ORIGINALARTIST"_L1
        || field == "TEXT"_L1 || field == "TOLY"_L1 || field == "TOPE"_L1;
}

bool isLyricsField(const QString& field)
{
    return field == "LYRICS"_L1 || field == "SYNCEDLYRICS"_L1 || field == "SYNCED LYRICS"_L1
        || field == "UNSYNCEDLYRICS"_L1 || field == "UNSYNCED LYRICS"_L1 || field == "UNSYNCHRONIZEDLYRICS"_L1
        || field == "UNSYNCHRONIZED LYRICS"_L1 || field == "USLT"_L1 || field == "SYLT"_L1;
}
} // namespace

std::optional<TimedMetadata> parseTimedMetadata(QByteArrayView data)
{
    if(data.size() < 10 || data.first(3) != "ID3") {
        if(data.size() < 5) {
            return {};
        }

        // FFmpeg 5 strips "ID3", the major version and revision from
        // MPEG-TS timed-ID3 packets. Reconstruct the missing prefix and
        // try both supported encodings.
        for(const char version : {char{3}, char{4}}) {
            QByteArray reconstructed{"ID3", 3};
            reconstructed.append(version);
            reconstructed.append(char{0});
            reconstructed.append(data);
            if(auto metadata = parseTimedMetadata(reconstructed)) {
                return metadata;
            }
        }

        return {};
    }

    const auto version = static_cast<Id3Version>(data.at(3));
    if(version != Id3Version::V2_3 && version != Id3Version::V2_4) {
        return {};
    }

    const qsizetype tagEnd = std::min<qsizetype>(data.size(), 10 + syncSafeUint32(data, 6));
    qsizetype offset{10};
    TimedMetadata metadata;

    while(offset + 10 <= tagEnd) {
        const QByteArrayView frameId = data.sliced(offset, 4);
        if(frameId == QByteArrayView{"\0\0\0\0", 4}) {
            break;
        }

        const uint32_t frameSize
            = version == Id3Version::V2_4 ? syncSafeUint32(data, offset + 4) : bigEndianUint32(data, offset + 4);
        offset += 10;
        if(frameSize == 0 || std::cmp_greater(frameSize, tagEnd - offset)) {
            break;
        }

        const QByteArrayView value = data.sliced(offset, frameSize);
        if(frameId == "TIT2") {
            metadata.title = decodeId3Text(value);
        }
        else if(frameId == "TPE1") {
            metadata.artist = decodeId3Text(value);
        }
        else if(frameId == "TRSN") {
            metadata.station = decodeId3Text(value);
        }
        offset += frameSize;
    }

    if(metadata.title.isEmpty() && metadata.artist.isEmpty() && metadata.station.isEmpty()) {
        return {};
    }
    return metadata;
}

QStringList splitStandardField(const QString& field, const QStringList& values, bool splitSemicolonSeparated)
{
    const QString upperField = field.toUpper();

    if(isSlashSeparatedField(upperField)) {
        return splitSlashSeparatedValues(values);
    }

    if(splitSemicolonSeparated && upperField == QLatin1StringView{Tag::Genre}) {
        return splitValues(values, u';');
    }

    return values;
}

QStringList splitExtraField(const QString& field, const QStringList& values, bool splitSemicolonSeparated)
{
    const QString upperField = field.toUpper();

    if(isSlashSeparatedExtraField(upperField)) {
        return splitSlashSeparatedValues(values);
    }

    if(isLyricsField(upperField)) {
        return values;
    }

    return splitSemicolonSeparated ? splitValues(values, u';') : values;
}
} // namespace Fooyin::Id3Utils
