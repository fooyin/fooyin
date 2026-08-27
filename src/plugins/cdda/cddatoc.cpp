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
 */

#include "cddatoc.h"

#include <QCoreApplication>

#include <QByteArray>
#include <QCryptographicHash>
#include <QLoggingCategory>
#include <QStringList>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(CDDA_TOC, "fy.cdda.toc")

namespace Fooyin::Cdda {
namespace {
std::expected<uint64_t, QString> checkedMultiply(uint64_t value, uint64_t multiplier)
{
    if(multiplier != 0 && value > std::numeric_limits<uint64_t>::max() / multiplier) {
        return std::unexpected(QCoreApplication::translate("Fooyin::Cdda::CdToc", "CD sector conversion overflow"));
    }
    return value * multiplier;
}

uint32_t musicBrainzOffset(int lba)
{
    return static_cast<uint32_t>(lba + LeadInSectors);
}

const char* cdTocErrorDescription(CdTocError error)
{
    switch(error) {
        case CdTocError::InvalidTrackNumberRange:
            return "invalid track number range";
        case CdTocError::MissingTracks:
            return "missing declared tracks";
        case CdTocError::InvalidLeadout:
            return "invalid leadout";
        case CdTocError::LeadoutTooLarge:
            return "leadout exceeds the supported range";
        case CdTocError::InvalidTrackNumbers:
            return "track numbers are missing, duplicated, or out of order";
        case CdTocError::InvalidTrackSectorRange:
            return "invalid track sector range";
        case CdTocError::NoncontiguousTrackSectorRanges:
            return "noncontiguous track sector ranges";
        case CdTocError::TrackBeyondLeadout:
            return "track extends beyond the leadout";
        case CdTocError::FinalTrackBeforeLeadout:
            return "final track does not end at the leadout";
        case CdTocError::NoAudioTracks:
            return "no audio tracks";
    }
    return "unknown error";
}

std::unexpected<CdTocError> invalidToc(CdTocError error)
{
    qCWarning(CDDA_TOC) << "Invalid CD TOC:" << cdTocErrorDescription(error);
    return std::unexpected(error);
}
} // namespace

QString invalidTocUserMessage()
{
    return QCoreApplication::translate("Fooyin::Cdda::CdToc", "The CD has an invalid TOC");
}

std::expected<void, CdTocError> validateToc(const CdToc& toc)
{
    if(toc.firstTrackNumber < 1 || toc.lastTrackNumber < toc.firstTrackNumber || toc.lastTrackNumber > MaximumTracks) {
        return invalidToc(CdTocError::InvalidTrackNumberRange);
    }

    const int expectedCount = toc.lastTrackNumber - toc.firstTrackNumber + 1;

    if(std::cmp_not_equal(toc.tracks.size(), expectedCount)) {
        return invalidToc(CdTocError::MissingTracks);
    }
    if(toc.leadoutSector <= 0) {
        return invalidToc(CdTocError::InvalidLeadout);
    }
    if(toc.leadoutSector > std::numeric_limits<int>::max() - LeadInSectors) {
        return invalidToc(CdTocError::LeadoutTooLarge);
    }

    bool hasAudio{false};
    int previousEnd{-1};

    for(int index{0}; std::cmp_less(index, toc.tracks.size()); ++index) {
        const CdTocTrack& track  = toc.tracks.at(index);
        const int expectedNumber = toc.firstTrackNumber + index;

        if(track.number != expectedNumber) {
            return invalidToc(CdTocError::InvalidTrackNumbers);
        }
        if(track.firstSector < 0 || track.endSectorExclusive <= track.firstSector) {
            return invalidToc(CdTocError::InvalidTrackSectorRange);
        }
        if(index > 0 && track.firstSector != previousEnd) {
            return invalidToc(CdTocError::NoncontiguousTrackSectorRanges);
        }
        if(track.endSectorExclusive > toc.leadoutSector) {
            return invalidToc(CdTocError::TrackBeyondLeadout);
        }

        previousEnd = track.endSectorExclusive;
        hasAudio |= track.isAudio;
    }

    if(toc.tracks.back().endSectorExclusive != toc.leadoutSector) {
        return invalidToc(CdTocError::FinalTrackBeforeLeadout);
    }
    if(!hasAudio) {
        return invalidToc(CdTocError::NoAudioTracks);
    }

    return {};
}

std::vector<CdTocTrack> audioTracks(const CdToc& toc)
{
    std::vector<CdTocTrack> result;
    result.reserve(toc.tracks.size());
    std::ranges::copy_if(toc.tracks, std::back_inserter(result), &CdTocTrack::isAudio);
    return result;
}

std::optional<CdTocTrack> audioTrackForSubsong(const CdToc& toc, int subsong)
{
    if(subsong < 0) {
        return {};
    }

    int audioIndex{0};
    for(const CdTocTrack& track : toc.tracks) {
        if(!track.isAudio) {
            continue;
        }
        if(audioIndex++ == subsong) {
            return track;
        }
    }
    return {};
}

std::expected<uint64_t, QString> framesForSectors(uint64_t sectors)
{
    return checkedMultiply(sectors, FramesPerSector);
}

std::expected<uint64_t, QString> bytesForSectors(uint64_t sectors)
{
    return checkedMultiply(sectors, BytesPerSector);
}

std::expected<uint64_t, QString> durationForSectors(uint64_t sectors)
{
    const auto milliseconds = checkedMultiply(sectors, 1000);
    if(!milliseconds) {
        return std::unexpected(milliseconds.error());
    }
    return *milliseconds / SectorsPerSecond;
}

std::expected<QString, CdTocError> musicBrainzDiscId(const CdToc& toc)
{
    if(const auto valid = validateToc(toc); !valid) {
        return std::unexpected(valid.error());
    }

    // MusicBrainz stores the leadout at offset 0 and track offsets at their
    // corresponding 1-based tracknumbers
    std::array<uint32_t, MaximumTracks + 1> offsets{};
    offsets[0] = musicBrainzOffset(toc.leadoutSector);
    for(const CdTocTrack& track : toc.tracks) {
        offsets.at(track.number) = musicBrainzOffset(track.firstSector);
    }

    QByteArray input;
    input.reserve(4 + (offsets.size() * 8));

    input += QByteArray::number(toc.firstTrackNumber, 16).rightJustified(2, '0').toUpper();
    input += QByteArray::number(toc.lastTrackNumber, 16).rightJustified(2, '0').toUpper();
    for(const uint32_t offset : offsets) {
        input += QByteArray::number(offset, 16).rightJustified(8, '0').toUpper();
    }

    QByteArray discId = QCryptographicHash::hash(input, QCryptographicHash::Sha1).toBase64();
    discId.replace('+', '.');
    discId.replace('/', '_');
    discId.replace('=', '-');

    return QString::fromLatin1(discId);
}

std::expected<QString, CdTocError> musicBrainzToc(const CdToc& toc)
{
    if(const auto valid = validateToc(toc); !valid) {
        return std::unexpected(valid.error());
    }

    QStringList parts{QString::number(toc.firstTrackNumber), QString::number(toc.lastTrackNumber),
                      QString::number(musicBrainzOffset(toc.leadoutSector))};
    parts.reserve(3 + toc.tracks.size());

    for(const CdTocTrack& track : toc.tracks) {
        parts.push_back(QString::number(musicBrainzOffset(track.firstSector)));
    }

    return parts.join(u'+');
}
} // namespace Fooyin::Cdda
