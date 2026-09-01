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

#include <core/engine/verification/audioverifier.h>

#include <core/constants.h>
#include <core/engine/verification/accuraterip.h>
#include <utils/scopeguard.h>

#include <QCryptographicHash>

#include <limits>
#include <zlib.h>

using namespace Qt::StringLiterals;

constexpr size_t TargetReadBytes = 256UL * 1024;

namespace Fooyin::AudioVerifier {
namespace {
bool shouldCancel(const Request& request)
{
    return request.cancelCallback && request.cancelCallback();
}

uint64_t framesForDuration(uint64_t durationMs, int sampleRate)
{
    if(sampleRate <= 0) {
        return 0;
    }

    const auto rate = static_cast<uint64_t>(sampleRate);
    if(durationMs > std::numeric_limits<uint64_t>::max() / rate) {
        return std::numeric_limits<uint64_t>::max();
    }

    return durationMs * rate / 1000;
}

AudioBuffer trimBuffer(const AudioBuffer& buffer, uint64_t frames)
{
    if(!buffer.isValid() || std::cmp_greater_equal(frames, buffer.frameCount())) {
        return buffer;
    }

    const auto bytes = static_cast<size_t>(buffer.format().bytesForFrames(static_cast<int>(frames)));
    return {buffer.constData().first(bytes), buffer.format(), buffer.startTime()};
}

std::optional<uint64_t> cueSectorProperty(const Track& track, const char* name)
{
    const auto properties   = track.extraProperties();
    const auto* const value = properties.find(QString::fromLatin1(name));
    if(!value) {
        return {};
    }

    bool ok{false};
    const uint64_t sector = value->toULongLong(&ok);
    return ok ? std::optional<uint64_t>{sector} : std::optional<uint64_t>{};
}

AudioBuffer removeLeadingFrames(const AudioBuffer& buffer, uint64_t frames)
{
    if(!buffer.isValid() || frames == 0) {
        return buffer;
    }

    if(std::cmp_greater_equal(frames, buffer.frameCount())) {
        return {};
    }

    const size_t offset = static_cast<size_t>(buffer.format().bytesForFrames(static_cast<int>(frames)));
    return {buffer.constData().subspan(offset), buffer.format(), buffer.startTime()};
}

AudioVerificationResult verifyTrack(const Request& request, const Track& track, int index)
{
    if(track.isRemote()) {
        return {.track    = track,
                .format   = {},
                .md5      = {},
                .crc32    = 0,
                .error    = u"Remote streams cannot be verified"_s,
                .warnings = {}};
    }

    AudioDecoder::DecoderOptions options = AudioDecoder::NoLooping | AudioDecoder::ForConversion;
    if(request.verifyIntegrity) {
        options |= AudioDecoder::VerifyIntegrity;
    }

    auto loaded = request.audioLoader->loadDecoderForTrack(track, options);
    if(!loaded.decoder || !loaded.format) {
        return {
            .track = track, .format = {}, .md5 = {}, .crc32 = 0, .error = u"No decoder available"_s, .warnings = {}};
    }

    loaded.decoder->start();

    const auto stopDecoder    = scopeGuard([&loaded] { loaded.decoder->stop(); });
    const auto cueStartSector = cueSectorProperty(track, Constants::CueIndex01Sector);
    if(track.offset() > 0 && !cueStartSector) {
        if(!loaded.decoder->isSeekable()) {
            return {.track    = track,
                    .format   = *loaded.format,
                    .md5      = {},
                    .crc32    = 0,
                    .error    = u"Decoder cannot seek to the track segment"_s,
                    .warnings = {}};
        }
        loaded.decoder->seek(track.offset());
    }

    uint64_t framesToSkip = cueStartSector.value_or(0) * AccurateRip::FramesPerSector;
    std::optional<uint64_t> framesRemaining;

    if(const auto cueEndSector = cueSectorProperty(track, Constants::CueEndSector); cueStartSector && cueEndSector) {
        if(*cueEndSector <= *cueStartSector) {
            return {.track    = track,
                    .format   = *loaded.format,
                    .md5      = {},
                    .crc32    = 0,
                    .error    = u"Invalid CUE track boundaries"_s,
                    .warnings = {}};
        }
        framesRemaining = (*cueEndSector - *cueStartSector) * AccurateRip::FramesPerSector;
    }
    else if(track.isBoundedSegment() && track.duration() > 0 && !cueStartSector) {
        framesRemaining = framesForDuration(track.duration(), loaded.format->sampleRate());
    }

    bool failed{false};
    bool complete{false};
    uint64_t decodedFrames{0};
    QString error;
    QCryptographicHash md5{QCryptographicHash::Md5};
    uLong crc32Value = ::crc32(0L, Z_NULL, 0);

    if(request.observer) {
        request.observer->trackStarted(track, *loaded.format);
    }

    const auto finishObserver = scopeGuard([&] {
        if(request.observer) {
            request.observer->trackFinished(track, !failed && complete);
        }
    });

    while(true) {
        if(shouldCancel(request)) {
            loaded.decoder->requestAbort();
            return {.track         = track,
                    .status        = AudioVerificationStatus::Cancelled,
                    .format        = *loaded.format,
                    .decodedFrames = decodedFrames,
                    .md5           = {},
                    .crc32         = 0,
                    .error         = u"Verification cancelled"_s,
                    .warnings      = loaded.decoder->takeWarnings()};
        }

        auto read = loaded.decoder->readAudio(TargetReadBytes);
        if(read.status == AudioDecoder::ReadStatus::NeedMoreInput) {
            continue;
        }

        if(read.status == AudioDecoder::ReadStatus::Error) {
            failed = true;
            error  = read.error.isEmpty() ? u"Decoder error"_s : std::move(read.error);
            break;
        }

        if(read.status == AudioDecoder::ReadStatus::EndOfStream) {
            complete = framesToSkip == 0 && (!framesRemaining || *framesRemaining == 0);
            if(!complete) {
                failed = true;
                error  = u"Decoder ended before the track segment was complete"_s;
            }
            break;
        }

        AudioBuffer buffer{std::move(read.buffer)};
        if(!buffer.isValid()) {
            failed = true;
            error  = u"Decoder returned invalid audio"_s;
            break;
        }

        if(framesToSkip > 0) {
            const uint64_t skipped = std::min<uint64_t>(framesToSkip, static_cast<uint64_t>(buffer.frameCount()));
            buffer                 = removeLeadingFrames(buffer, skipped);
            framesToSkip -= skipped;
            if(!buffer.isValid()) {
                continue;
            }
        }

        if(framesRemaining) {
            buffer = trimBuffer(buffer, *framesRemaining);
            *framesRemaining -= static_cast<uint64_t>(buffer.frameCount());
        }

        decodedFrames += static_cast<uint64_t>(buffer.frameCount());
        const auto bytes = buffer.constData();
        md5.addData(QByteArrayView{reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())});
        crc32Value = ::crc32(crc32Value, reinterpret_cast<const Bytef*>(bytes.data()), static_cast<uInt>(bytes.size()));
        if(request.observer && buffer.frameCount() > 0) {
            request.observer->sourceAudio(track, buffer);
        }

        if(request.progressCallback) {
            request.progressCallback({.trackIndex = index,
                                      .trackCount = static_cast<int>(request.tracks.size()),
                                      .track      = track,
                                      .positionMs = buffer.endTime()});
        }

        if(framesRemaining && *framesRemaining == 0) {
            complete = true;
            break;
        }
    }

    return {.track         = track,
            .status        = failed ? AudioVerificationStatus::Failed : AudioVerificationStatus::Succeeded,
            .format        = *loaded.format,
            .decodedFrames = decodedFrames,
            .md5           = !failed && complete ? md5.result() : QByteArray{},
            .crc32         = !failed && complete ? static_cast<uint32_t>(crc32Value) : 0,
            .error         = std::move(error),
            .warnings      = loaded.decoder->takeWarnings()};
}
} // namespace

std::vector<AudioVerificationResult> run(const Request& request)
{
    if(!request.audioLoader) {
        return {};
    }

    std::vector<AudioVerificationResult> results;
    results.reserve(request.tracks.size());

    for(int index{0}; std::cmp_less(index, request.tracks.size()); ++index) {
        if(shouldCancel(request)) {
            for(; std::cmp_less(index, request.tracks.size()); ++index) {
                results.push_back({.track    = request.tracks.at(index),
                                   .status   = AudioVerificationStatus::Cancelled,
                                   .format   = {},
                                   .md5      = {},
                                   .crc32    = 0,
                                   .error    = u"Verification cancelled"_s,
                                   .warnings = {}});
            }
            break;
        }
        results.push_back(verifyTrack(request, request.tracks.at(index), index));
    }

    return results;
}
} // namespace Fooyin::AudioVerifier
