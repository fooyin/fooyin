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

#include "cddadecoder.h"

#include "cddatoc.h"
#include "cddaurl.h"

#include <QLoggingCategory>

#include <algorithm>
#include <limits>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(CDDA_DECODER, "fy.cdda.decoder")

constexpr int SectorsPerRead = 16;
constexpr int BytesPerFrame  = 4;

namespace Fooyin::Cdda {
CddaDecoder::CddaDecoder(std::shared_ptr<CdDriveManager> driveManager,
                         std::shared_ptr<const CdDriveSettingsProvider> settingsProvider)
    : m_driveManager{std::move(driveManager)}
    , m_settingsProvider{std::move(settingsProvider)}
    , m_format{SampleFormat::S16, 44100, 2}
    , m_subsong{-1}
    , m_nextSector{0}
    , m_nextLogicalFrame{0}
    , m_consumedFrames{0}
    , m_bufferPosition{0}
    , m_activeSession{nullptr}
    , m_initialised{false}
    , m_readFailed{false}
    , m_forConversion{false}
    , m_readOffsetFrames{0}
{ }

CddaDecoder::~CddaDecoder()
{
    stop();
}

QStringList CddaDecoder::extensions() const
{
    return {};
}

QStringList CddaDecoder::supportedSchemes() const
{
    return {QString::fromLatin1(Scheme)};
}

bool CddaDecoder::isSeekable() const
{
    return m_initialised;
}

bool CddaDecoder::allowsConcurrentDecoding() const
{
    return false;
}

int CddaDecoder::playbackPrebufferMs() const
{
    return 1000;
}

std::optional<AudioFormat> CddaDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    stop();
    m_error.clear();
    m_warnings.clear();

    const auto discId = discIdFromCddaUrl(source.filepath);
    if(!discId || source.filepath != track.filepath()) {
        qCWarning(CDDA_DECODER) << "Invalid audio CD decoder source:"
                                << "source=" << source.filepath << "track=" << track.filepath();
        return {};
    }

    auto observation = m_driveManager->resolveDisc(*discId);
    if(!observation) {
        qCWarning(CDDA_DECODER) << "Failed to resolve audio CD source:"
                                << "source=" << source.filepath
                                << "errorCode=" << static_cast<int>(observation.error().code)
                                << "error=" << observation.error().message;
        return {};
    }
    if(!observation->toc) {
        qCWarning(CDDA_DECODER) << "Resolved audio CD source has no TOC:"
                                << "source=" << source.filepath << "drive=" << observation->drive.id;
        return {};
    }

    const auto tocTrack = audioTrackForSubsong(*observation->toc, track.subsong());
    if(!tocTrack) {
        qCWarning(CDDA_DECODER) << "Audio CD track index is out of range:"
                                << "source=" << source.filepath << "subsong=" << track.subsong();
        return {};
    }

    m_discId           = *discId;
    m_observedDriveId  = observation->drive.id;
    m_toc              = *observation->toc;
    m_track            = *tocTrack;
    m_subsong          = track.subsong();
    m_nextSector       = m_track.firstSector;
    m_nextLogicalFrame = 0;
    m_consumedFrames   = 0;
    m_forConversion    = options.testFlag(ForConversion);
    m_initialised      = true;

    return m_format;
}

void CddaDecoder::stop()
{
    releaseLease();

    m_discId.clear();
    m_observedDriveId.clear();
    m_toc              = {};
    m_track            = {};
    m_subsong          = -1;
    m_nextSector       = 0;
    m_nextLogicalFrame = 0;
    m_consumedFrames   = 0;
    m_buffer.clear();
    m_bufferPosition   = 0;
    m_initialised      = false;
    m_readFailed       = false;
    m_forConversion    = false;
    m_readOffsetFrames = 0;
}

void CddaDecoder::seek(uint64_t pos)
{
    if(!m_initialised) {
        return;
    }

    const auto trackSectors = static_cast<uint64_t>(m_track.endSectorExclusive - m_track.firstSector);
    const auto duration     = durationForSectors(trackSectors);
    const uint64_t clamped  = std::min(pos, duration.value_or(0));
    const uint64_t relativeSector
        = duration && pos >= *duration
            ? trackSectors
            : std::min(trackSectors, clamped > std::numeric_limits<uint64_t>::max() / SectorsPerSecond
                                         ? trackSectors
                                         : (clamped * SectorsPerSecond) / 1000);

    m_nextSector       = m_track.firstSector + static_cast<int>(relativeSector);
    m_nextLogicalFrame = relativeSector * FramesPerSector;
    m_consumedFrames   = relativeSector * FramesPerSector;
    m_buffer.clear();
    m_bufferPosition = 0;
    m_error.clear();
    m_readFailed = false;
}

AudioDecoder::ReadResult CddaDecoder::readAudio(size_t bytes)
{
    if(!m_initialised) {
        return ReadResult::errorResult(tr("Audio CD decoder is not initialised"));
    }

    if(m_readFailed) {
        return ReadResult::errorResult(m_error);
    }
    if(abortToken().stop_requested()) {
        return ReadResult::errorResult(tr("Audio CD read was cancelled"));
    }

    const size_t alignedRequest = bytes - (bytes % BytesPerFrame);
    if(alignedRequest == 0) {
        return ReadResult::needMoreInput();
    }

    static constexpr size_t maximumRequest = std::numeric_limits<int>::max() - (BytesPerFrame - 1);
    const auto request                     = static_cast<qsizetype>(std::min(alignedRequest, maximumRequest));

    AudioBuffer output{m_format, currentTimestamp()};
    output.reserve(request);

    while(output.byteCount() < request) {
        if(m_forConversion && output.byteCount() > 0 && m_bufferPosition >= m_buffer.size()) {
            break;
        }

        const qsizetype buffered = m_buffer.size() - m_bufferPosition;
        if(buffered > 0) {
            const qsizetype count = std::min(buffered, request - output.byteCount());
            output.append(reinterpret_cast<const std::byte*>(m_buffer.constData() + m_bufferPosition), count);
            m_bufferPosition += count;
            m_consumedFrames += static_cast<uint64_t>(count / BytesPerFrame);
            if(m_bufferPosition == m_buffer.size()) {
                m_buffer.clear();
                m_bufferPosition = 0;
            }

            continue;
        }

        const uint64_t trackFrames
            = static_cast<uint64_t>(m_track.endSectorExclusive - m_track.firstSector) * FramesPerSector;
        if((m_forConversion && m_nextLogicalFrame >= trackFrames)
           || (!m_forConversion && m_nextSector >= m_track.endSectorExclusive)) {
            break;
        }

        if(!acquireLease()) {
            m_readFailed = true;
            break;
        }

        const qsizetype remainingBytes = request - output.byteCount();
        if(m_forConversion) {
            const int requestedFrames
                = static_cast<int>(std::min<uint64_t>(remainingBytes / BytesPerFrame, MaximumFramesPerPolicyRead));
            const int frameCount
                = static_cast<int>(std::min<uint64_t>(requestedFrames, trackFrames - m_nextLogicalFrame));
            auto read = m_sectorReader->readCorrectedFrames(m_toc, m_track, m_nextLogicalFrame, frameCount,
                                                            m_readOffsetFrames);
            if(!read) {
                setReadError(read.error());
                break;
            }

            m_nextLogicalFrame += static_cast<uint64_t>(read->framesRead);
            m_buffer         = std::move(read->pcm);
            m_bufferPosition = 0;
            continue;
        }

        const int requestedSectors
            = std::clamp(static_cast<int>((remainingBytes / BytesPerSector) + (remainingBytes % BytesPerSector != 0)),
                         1, SectorsPerRead);
        const int count = std::min(requestedSectors, m_track.endSectorExclusive - m_nextSector);
        auto read       = m_lease->session()->readAudioSectors(m_nextSector, count);
        if(!read) {
            setReadError(read.error());
            break;
        }

        if(const auto valid = validateSectorRead(*read, count); !valid) {
            setReadError(valid.error());
            break;
        }

        if(read->sectorsRead == 0) {
            setReadError(CdError{
                .code = CdDriveError::ReadFailed, .message = tr("CD drive returned no audio data"), .platformCode = 0});
            break;
        }

        m_nextSector += read->sectorsRead;
        m_buffer         = std::move(read->pcm);
        m_bufferPosition = 0;
    }

    if(output.byteCount() > 0) {
        return ReadResult::data(std::move(output));
    }

    if(m_readFailed) {
        qCWarning(CDDA_DECODER) << "Audio CD read failed:"
                                << "subsong=" << m_subsong << "nextSector=" << m_nextSector
                                << "endSectorExclusive=" << m_track.endSectorExclusive << "error=" << m_error;
        return ReadResult::errorResult(m_error);
    }

    releaseLease();
    return ReadResult::endOfStream();
}

AudioBuffer CddaDecoder::readBuffer(size_t bytes)
{
    auto result = readAudio(bytes);
    return result.status == ReadStatus::DecodedAudio ? std::move(result.buffer) : AudioBuffer{};
}

QStringList CddaDecoder::takeWarnings()
{
    if(m_sectorReader) {
        m_warnings.append(m_sectorReader->takeWarnings());
    }
    return std::exchange(m_warnings, {});
}

void CddaDecoder::interruptRead()
{
    const std::scoped_lock lock{m_sessionMutex};
    if(m_activeSession) {
        m_activeSession->cancel();
    }
}

bool CddaDecoder::acquireLease()
{
    if(m_lease) {
        return true;
    }

    if(abortToken().stop_requested()) {
        m_error = tr("Audio CD read was cancelled");
        return false;
    }

    auto lease = m_driveManager->acquire(m_discId);
    if(!lease) {
        m_error = lease.error().message;
        qCWarning(CDDA_DECODER) << "Failed to acquire audio CD drive:"
                                << "subsong=" << m_subsong << "error=" << m_error;
        return false;
    }

    const auto actualTrack = audioTrackForSubsong(lease->toc(), m_subsong);
    if(!actualTrack || *actualTrack != m_track) {
        m_error = tr("The disc in the CD drive has changed");
        return false;
    }

    m_lease.emplace(std::move(*lease));
    qCDebug(CDDA_DECODER) << "Acquired audio CD drive:"
                          << "subsong=" << m_subsong << "drive=" << m_lease->drive().id;

    if(m_forConversion) {
        const CdDriveSettings settings = normaliseDriveSettings(m_settingsProvider->settingsForDrive(m_lease->drive()));

        m_readOffsetFrames = settings.readOffsetFrames;

        if(settings.readSpeedLimit > 0 && m_lease->drive().supportsSpeedLimit) {
            if(const auto speed = m_lease->session()->setReadSpeed(settings.readSpeedLimit); !speed) {
                m_warnings.push_back(tr("Could not limit CD read speed: %1").arg(speed.error().message));
            }
        }

        m_sectorReader = std::make_unique<CddaSectorReader>(m_lease->session(), settings.security, abortToken());
        qCDebug(CDDA_DECODER) << "Configured audio CD extraction:"
                              << "subsong=" << m_subsong << "security=" << static_cast<int>(settings.security)
                              << "readOffsetFrames=" << m_readOffsetFrames
                              << "readSpeedLimit=" << settings.readSpeedLimit;

        if(!m_observedDriveId.isEmpty() && m_observedDriveId != m_lease->drive().id) {
            m_warnings.push_back(
                tr("The selected CD drive became unavailable; extraction settings for %1 are being used")
                    .arg(m_lease->drive().displayName));
        }
    }

    {
        const std::scoped_lock lock{m_sessionMutex};
        m_activeSession = m_lease->session();
    }

    return true;
}

void CddaDecoder::releaseLease()
{
    if(m_sectorReader) {
        m_warnings.append(m_sectorReader->takeWarnings());
        m_sectorReader.reset();
    }

    {
        const std::scoped_lock lock{m_sessionMutex};
        m_activeSession = nullptr;
    }

    m_lease.reset();
}

void CddaDecoder::setReadError(CdError error)
{
    if(m_lease) {
        m_driveManager->invalidateDrive(m_lease->drive().id);
    }

    m_error      = std::move(error.message);
    m_readFailed = true;

    qCWarning(CDDA_DECODER) << "Audio CD device read error:"
                            << "subsong=" << m_subsong << "nextSector=" << m_nextSector
                            << "endSectorExclusive=" << m_track.endSectorExclusive << "error=" << m_error;

    releaseLease();
}

uint64_t CddaDecoder::currentTimestamp() const
{
    return (m_consumedFrames * 1000) / static_cast<uint64_t>(m_format.sampleRate());
}
} // namespace Fooyin::Cdda
