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

#include "cddasectorreader.h"

#include "cddatoc.h"

#include <QLoggingCategory>

#include <limits>
#include <optional>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(CDDA_SECTOR_READER, "fy.cdda.sectorreader")

constexpr auto BytesPerPcmFrame = 4;

namespace Fooyin::Cdda {
namespace {
CdError internalReadError()
{
    return {.code = CdDriveError::ReadFailed, .message = {}, .platformCode = 0};
}

} // namespace

CddaSectorReader::CddaSectorReader(CdDriveSession* session, CdRippingSecurity security, std::stop_token stopToken)
    : m_session{session}
    , m_security{security}
    , m_stopToken{std::move(stopToken)}
{ }

std::expected<CdSectorRead, CdError> CddaSectorReader::readAudioSectors(int firstSector, int count)
{
    if(firstSector < 0 || count < 0 || count > MaximumSectorsPerPolicyRead
       || firstSector > std::numeric_limits<int>::max() - count) {
        qCWarning(CDDA_SECTOR_READER) << "Rejected invalid sector request:"
                                      << "firstSector=" << firstSector << "count=" << count
                                      << "maximum=" << MaximumSectorsPerPolicyRead;
        return std::unexpected(internalReadError());
    }
    if(const auto cancelled = cancellationError()) {
        return std::unexpected(*cancelled);
    }
    if(count == 0) {
        return CdSectorRead{};
    }

    return readExact(firstSector, count);
}

std::expected<CdFrameRead, CdError> CddaSectorReader::readCorrectedFrames(const CdToc& toc, const CdTocTrack& track,
                                                                          uint64_t firstFrame, int frameCount,
                                                                          int readOffsetFrames)
{
    if(const auto valid = validateToc(toc); !valid) {
        return std::unexpected(
            CdError{.code = CdDriveError::NotAudioDisc, .message = invalidTocUserMessage(), .platformCode = 0});
    }

    const auto trackIt = std::ranges::find(toc.tracks, track);
    if(trackIt == toc.tracks.end() || !track.isAudio || frameCount < 0 || frameCount > MaximumFramesPerPolicyRead) {
        qCWarning(CDDA_SECTOR_READER) << "Rejected invalid track-frame request:"
                                      << "track=" << track.number << "isAudio=" << track.isAudio
                                      << "inToc=" << (trackIt != toc.tracks.end()) << "frameCount=" << frameCount
                                      << "maximum=" << MaximumFramesPerPolicyRead;
        return std::unexpected(internalReadError());
    }

    const uint64_t trackFrames = static_cast<uint64_t>(track.endSectorExclusive - track.firstSector) * FramesPerSector;
    if(firstFrame > trackFrames || static_cast<uint64_t>(frameCount) > trackFrames - firstFrame) {
        qCWarning(CDDA_SECTOR_READER) << "Rejected out-of-range track-frame request:"
                                      << "track=" << track.number << "firstFrame=" << firstFrame
                                      << "frameCount=" << frameCount << "trackFrames=" << trackFrames;
        return std::unexpected(internalReadError());
    }
    if(const auto cancelled = cancellationError()) {
        return std::unexpected(*cancelled);
    }
    if(frameCount == 0) {
        return CdFrameRead{};
    }

    const int trackIndex = static_cast<int>(std::distance(toc.tracks.begin(), trackIt));
    int audioFirst{trackIndex};
    while(audioFirst > 0 && toc.tracks.at(audioFirst - 1).isAudio) {
        --audioFirst;
    }
    int audioLast{trackIndex};
    while(std::cmp_less(audioLast + 1, toc.tracks.size()) && toc.tracks.at(audioLast + 1).isAudio) {
        ++audioLast;
    }

    const int64_t readableStart = static_cast<int64_t>(toc.tracks.at(audioFirst).firstSector) * FramesPerSector;
    const int64_t readableEnd   = static_cast<int64_t>(toc.tracks.at(audioLast).endSectorExclusive) * FramesPerSector;
    const int64_t logicalStart
        = (static_cast<int64_t>(track.firstSector) * FramesPerSector) + static_cast<int64_t>(firstFrame);
    const int64_t correctedStart = logicalStart + static_cast<int64_t>(readOffsetFrames);
    const int64_t correctedEnd   = correctedStart + frameCount;
    const int prefixFrames
        = static_cast<int>(std::clamp(readableStart - correctedStart, int64_t{0}, static_cast<int64_t>(frameCount)));
    const int suffixFrames = static_cast<int>(
        std::clamp(correctedEnd - readableEnd, int64_t{0}, static_cast<int64_t>(frameCount - prefixFrames)));
    const int physicalFrames = frameCount - prefixFrames - suffixFrames;

    QStringList correctionWarnings;
    CdFrameRead output;
    output.pcm.reserve(static_cast<qsizetype>(frameCount) * BytesPerPcmFrame);

    if(prefixFrames > 0) {
        output.pcm.append(QByteArray(static_cast<qsizetype>(prefixFrames) * BytesPerPcmFrame, '\0'));
        correctionWarnings.push_back(
            tr("CD read offset correction padded %Ln frame(s) with silence before the readable audio range", nullptr,
               prefixFrames));
    }

    if(physicalFrames > 0) {
        const int64_t physicalStart  = std::max(correctedStart, readableStart);
        const int firstSector        = static_cast<int>(physicalStart / FramesPerSector);
        const int frameInSector      = static_cast<int>(physicalStart % FramesPerSector);
        const int64_t physicalEnd    = physicalStart + physicalFrames;
        const int endSectorExclusive = static_cast<int>((physicalEnd + FramesPerSector - 1) / FramesPerSector);

        QByteArray sectorPcm;
        sectorPcm.reserve(static_cast<qsizetype>(endSectorExclusive - firstSector) * BytesPerSector);

        int sector{firstSector};
        while(sector < endSectorExclusive) {
            const int sectorCount = std::min(endSectorExclusive - sector, MaximumSectorsPerPolicyRead);
            auto read             = readAudioSectors(sector, sectorCount);
            if(!read) {
                return std::unexpected(read.error());
            }
            sectorPcm.append(read->pcm);
            sector += read->sectorsRead;
        }

        const qsizetype byteOffset = static_cast<qsizetype>(frameInSector) * BytesPerPcmFrame;
        const qsizetype byteCount  = static_cast<qsizetype>(physicalFrames) * BytesPerPcmFrame;
        if(byteOffset > sectorPcm.size() || byteCount > sectorPcm.size() - byteOffset) {
            qCWarning(CDDA_SECTOR_READER)
                << "Corrected CD read produced an invalid PCM window:"
                << "bufferBytes=" << sectorPcm.size() << "byteOffset=" << byteOffset << "byteCount=" << byteCount;
            return std::unexpected(internalReadError());
        }

        output.pcm.append(sectorPcm.constData() + byteOffset, byteCount);
    }

    if(suffixFrames > 0) {
        output.pcm.append(QByteArray(static_cast<qsizetype>(suffixFrames) * BytesPerPcmFrame, '\0'));
        correctionWarnings.push_back(
            tr("CD read offset correction padded %Ln frame(s) with silence after the readable audio range", nullptr,
               suffixFrames));
    }

    if(output.pcm.size() != static_cast<qsizetype>(frameCount) * BytesPerPcmFrame) {
        qCWarning(CDDA_SECTOR_READER) << "Corrected CD read produced an incomplete PCM buffer:"
                                      << "actualBytes=" << output.pcm.size()
                                      << "expectedBytes=" << static_cast<qsizetype>(frameCount) * BytesPerPcmFrame;
        return std::unexpected(internalReadError());
    }

    output.framesRead = frameCount;
    m_warnings.append(correctionWarnings);
    return output;
}

QStringList CddaSectorReader::takeWarnings()
{
    return std::exchange(m_warnings, {});
}

std::expected<CdSectorRead, CdError> CddaSectorReader::readExact(int firstSector, int count)
{
    CdSectorRead output;
    output.pcm.reserve(static_cast<qsizetype>(count) * BytesPerSector);

    while(output.sectorsRead < count) {
        if(const auto cancelled = cancellationError()) {
            return std::unexpected(*cancelled);
        }

        const int remaining = count - output.sectorsRead;
        const int requested = std::min(remaining, MaximumSectorsPerPolicyRead);

        auto read = m_session->readAudioSectorsSecure(firstSector + output.sectorsRead, requested, m_security);
        if(!read) {
            return std::unexpected(read.error());
        }
        if(const auto valid = validateSectorRead(*read, requested); !valid) {
            return std::unexpected(valid.error());
        }
        if(const auto cancelled = cancellationError()) {
            return std::unexpected(*cancelled);
        }
        if(read->sectorsRead == 0) {
            qCWarning(CDDA_SECTOR_READER) << "CD drive made no progress while completing a sector request:"
                                          << "firstSector=" << firstSector << "requestedSectors=" << count
                                          << "completedSectors=" << output.sectorsRead;
            return std::unexpected(internalReadError());
        }

        output.pcm.append(read->pcm);
        output.sectorsRead += read->sectorsRead;
        m_warnings.append(m_session->takeWarnings());
    }

    return output;
}

std::optional<CdError> CddaSectorReader::cancellationError() const
{
    if(!m_stopToken.stop_requested()) {
        return {};
    }
    return CdError{.code = CdDriveError::Cancelled, .message = tr("CD extraction was cancelled"), .platformCode = 0};
}
} // namespace Fooyin::Cdda
