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

#include "decodingcontroller.h"

#include "audioutils.h"

#include <QLoggingCategory>
#include <QObject>

#include <algorithm>
#include <chrono>
#include <limits>

constexpr auto DecodeFillIntervalMs         = 20;
constexpr auto BaseDecodeFramesPerChunk     = 4096;
constexpr auto MaxDecodeFramesPerChunk      = 262144;
constexpr auto TargetChunkDurationMs        = 10;
constexpr auto BurstShortfallLogThresholdMs = 200;
constexpr auto BurstRecoveryMinChunks       = 8;
constexpr auto DecodeTimerGapWarnMs         = 500;
constexpr auto DecodeLoopDurationWarnMs     = 500;
constexpr auto DecodeHighWatermarkMaxRatio  = 0.99;
constexpr auto DecodeWatermarkMinHeadroomMs = 20;
constexpr auto DecodeWatermarkMinGapMs      = 30;

Q_DECLARE_LOGGING_CATEGORY(ENGINE)

namespace {
int streamCapacityMs(const Fooyin::AudioStreamPtr& stream)
{
    if(!stream) {
        return 0;
    }

    const int channels = stream->channelCount();
    const int rate     = stream->sampleRate();
    if(channels <= 0 || rate <= 0) {
        return 0;
    }

    const size_t capacitySamples = stream->writer().capacity();
    if(capacitySamples == 0) {
        return 0;
    }

    const auto frames = static_cast<uint64_t>(capacitySamples / static_cast<size_t>(channels));
    const uint64_t ms = (frames * 1000ULL) / static_cast<uint64_t>(rate);

    return static_cast<int>(std::min<uint64_t>(ms, static_cast<uint64_t>(std::numeric_limits<int>::max())));
}

std::pair<int, int> effectiveWatermarksForStream(const Fooyin::AudioStreamPtr& stream, int lowWatermarkMs,
                                                 int highWatermarkMs)
{
    const int low  = std::max(1, lowWatermarkMs);
    const int high = std::max(low, highWatermarkMs);

    const int capacityMs = streamCapacityMs(stream);
    if(capacityMs <= 0) {
        return {low, high};
    }

    const int maxHighByRatioMs
        = std::max(1, static_cast<int>(std::floor(static_cast<double>(capacityMs) * DecodeHighWatermarkMaxRatio)));
    const int maxHighByHeadroomMs = std::max(1, capacityMs - DecodeWatermarkMinHeadroomMs);
    const int maxHighMs           = std::max(1, std::min({capacityMs, maxHighByRatioMs, maxHighByHeadroomMs}));
    const int effectiveHigh       = std::clamp(high, 1, maxHighMs);

    const int configuredGap   = std::max(0, high - low);
    const int desiredGapMs    = std::max(configuredGap, DecodeWatermarkMinGapMs);
    const int effectiveGapMs  = std::min(desiredGapMs, effectiveHigh - 1);
    const int lowCeilForGapMs = effectiveHigh - effectiveGapMs;
    const int effectiveLow    = std::clamp(low, 1, lowCeilForGapMs);

    return {effectiveLow, effectiveHigh};
}

int clampMsToStreamCapacity(const Fooyin::AudioStreamPtr& stream, int valueMs, int minMs)
{
    const int clampedValue = std::max(minMs, valueMs);
    const int capacityMs   = streamCapacityMs(stream);
    if(capacityMs <= 0) {
        return clampedValue;
    }

    return std::clamp(clampedValue, minMs, capacityMs);
}

size_t targetSamplesForMs(const Fooyin::AudioStreamPtr& stream, int targetMs)
{
    if(!stream || targetMs <= 0) {
        return 0;
    }

    const int channels = stream->channelCount();
    const int rate     = stream->sampleRate();
    if(channels <= 0 || rate <= 0) {
        return 0;
    }

    const uint64_t samples
        = (static_cast<uint64_t>(targetMs) * static_cast<uint64_t>(rate) * static_cast<uint64_t>(channels)) / 1000ULL;
    return static_cast<size_t>(std::min<uint64_t>(samples, static_cast<uint64_t>(std::numeric_limits<size_t>::max())));
}

int adaptiveBurstChunkLimit(const Fooyin::AudioStreamPtr& stream, int fillTargetMs)
{
    static constexpr auto MinBurstChunks = 8;
    static constexpr auto MaxBurstChunks = 128;

    if(!stream) {
        return MinBurstChunks;
    }

    const int channels = stream->channelCount();
    if(channels <= 0) {
        return MinBurstChunks;
    }

    const size_t targetSamples   = targetSamplesForMs(stream, fillTargetMs);
    const size_t bufferedSamples = stream->bufferedSamples();
    if(targetSamples <= bufferedSamples) {
        return MinBurstChunks;
    }

    const int sampleRate = stream->sampleRate();
    if(sampleRate <= 0) {
        return MinBurstChunks;
    }

    const uint64_t desiredFrames
        = (static_cast<uint64_t>(sampleRate) * static_cast<uint64_t>(TargetChunkDurationMs)) / 1000ULL;
    const size_t framesPerChunk
        = static_cast<size_t>(std::clamp<uint64_t>(desiredFrames, static_cast<uint64_t>(BaseDecodeFramesPerChunk),
                                                   static_cast<uint64_t>(MaxDecodeFramesPerChunk)));

    const size_t deficitSamples  = targetSamples - bufferedSamples;
    const size_t samplesPerChunk = framesPerChunk * static_cast<size_t>(channels);
    if(samplesPerChunk == 0) {
        return MinBurstChunks;
    }

    const uint64_t neededChunks
        = (static_cast<uint64_t>(deficitSamples) + static_cast<uint64_t>(samplesPerChunk) - 1ULL)
        / static_cast<uint64_t>(samplesPerChunk);

    return static_cast<int>(std::clamp<uint64_t>(neededChunks, static_cast<uint64_t>(MinBurstChunks),
                                                 static_cast<uint64_t>(MaxBurstChunks)));
}

size_t decodeFramesPerChunk(const Fooyin::AudioStreamPtr& stream)
{
    if(!stream) {
        return BaseDecodeFramesPerChunk;
    }

    const int sampleRate = stream->sampleRate();
    if(sampleRate <= 0) {
        return BaseDecodeFramesPerChunk;
    }

    const uint64_t desiredFrames
        = (static_cast<uint64_t>(sampleRate) * static_cast<uint64_t>(TargetChunkDurationMs)) / 1000ULL;
    return static_cast<size_t>(std::clamp<uint64_t>(desiredFrames, static_cast<uint64_t>(BaseDecodeFramesPerChunk),
                                                    static_cast<uint64_t>(MaxDecodeFramesPerChunk)));
}

void incrementAtomicSaturating(std::atomic<uint64_t>& value)
{
    uint64_t current = value.load(std::memory_order_relaxed);
    while(current != std::numeric_limits<uint64_t>::max()
          && !value.compare_exchange_weak(current, current + 1ULL, std::memory_order_relaxed,
                                          std::memory_order_relaxed)) { }
}
} // namespace

namespace Fooyin {
DecodingController::DecodingController(QObject* timerHost)
    : m_timerHost{timerHost}
    , m_decodeTimerId{0}
    , m_lowWatermarkMs{200}
    , m_highWatermarkMs{400}
    , m_reserveTargetMs{0}
    , m_fillUntilTarget{false}
    , m_decodeReserveClampEvents{0}
    , m_decodeBurstCapShortfallEvents{0}
    , m_hasLastDecodeTimerTick{false}
    , m_decodeTimerGapLogActive{false}
{ }

void DecodingController::startDecoding()
{
    if(!isDecoding()) {
        start();
    }
    m_fillUntilTarget         = true;
    m_hasLastDecodeTimerTick  = false;
    m_decodeTimerGapLogActive = false;
    ensureDecodeTimerRunning();
}

void DecodingController::stopDecoding()
{
    stopDecodeTimer();
    stop();
    m_fillUntilTarget         = false;
    m_reserveTargetMs         = 0;
    m_hasLastDecodeTimerTick  = false;
    m_decodeTimerGapLogActive = false;
}

void DecodingController::stopDecodeTimer()
{
    if(m_decodeTimerId != 0 && m_timerHost) {
        m_timerHost->killTimer(m_decodeTimerId);
        m_decodeTimerId = 0;
    }
    m_hasLastDecodeTimerTick  = false;
    m_decodeTimerGapLogActive = false;
}

void DecodingController::ensureDecodeTimerRunning()
{
    if(m_decodeTimerId == 0 && m_timerHost) {
        m_decodeTimerId = m_timerHost->startTimer(DecodeFillIntervalMs, Qt::CoarseTimer);
    }
}

bool DecodingController::isDecodeTimerActive() const
{
    return m_decodeTimerId != 0;
}

void DecodingController::setBufferWatermarksMs(int lowWatermarkMs, int highWatermarkMs)
{
    const int low  = std::max(1, lowWatermarkMs);
    const int high = std::max(low, highWatermarkMs);

    m_lowWatermarkMs  = low;
    m_highWatermarkMs = high;

    if(m_reserveTargetMs > 0) {
        m_reserveTargetMs = std::max(m_reserveTargetMs, m_highWatermarkMs);
        m_reserveTargetMs = clampMsToStreamCapacity(activeStream(), m_reserveTargetMs, 0);
    }
}

void DecodingController::requestDecodeReserveMs(int reserveMs)
{
    const int requested        = std::max(0, reserveMs);
    const int clampedRequested = clampMsToStreamCapacity(activeStream(), requested, 0);
    m_reserveTargetMs          = std::max(m_reserveTargetMs, clampedRequested);

    if(requested > 0 && clampedRequested < requested) {
        incrementAtomicSaturating(m_decodeReserveClampEvents);
        qCWarning(ENGINE) << "Decode reserve clamped to stream capacity:" << clampedRequested
                          << "ms requested:" << requested << "ms";
    }
}

void DecodingController::clearDecodeReserve()
{
    m_reserveTargetMs = 0;
}

int DecodingController::lowWatermarkMs() const
{
    const auto effective = effectiveWatermarksForStream(activeStream(), m_lowWatermarkMs, m_highWatermarkMs);
    return effective.first;
}

int DecodingController::highWatermarkMs() const
{
    const auto effective = effectiveWatermarksForStream(activeStream(), m_lowWatermarkMs, m_highWatermarkMs);
    return effective.second;
}

AudioStreamPtr DecodingController::setupCrossfadeStream(int bufferLengthMs, const Engine::FadeCurve curve)
{
    auto stream = activeStream();
    if(!stream) {
        const AudioFormat& decoderFormat = format();
        const size_t bufferSamples
            = Audio::bufferSamplesFromMs(bufferLengthMs, decoderFormat.sampleRate(), decoderFormat.channelCount());
        stream = createStream(bufferSamples, curve);
        if(!stream) {
            return {};
        }

        stream->setTrack(track());
        setActiveStream(stream);

        if(startPosition() > 0) {
            seek(startPosition());
            syncStreamPosition();
        }
    }
    else {
        stream->setTrack(track());
        stream->setFadeCurve(curve);
    }

    if(!isDecoding()) {
        start();
    }

    return stream;
}

AudioStreamPtr DecodingController::prepareSeekStream(uint64_t seekPosMs, int bufferLengthMs,
                                                     const Engine::FadeCurve curve, const Track& track)
{
    clearDecodeReserve();
    seek(seekPosMs);

    const AudioFormat& decoderFormat = format();
    const size_t bufferSamples
        = Audio::bufferSamplesFromMs(bufferLengthMs, decoderFormat.sampleRate(), decoderFormat.channelCount());

    auto stream = createStream(bufferSamples, curve);
    if(!stream) {
        return {};
    }

    stream->setTrack(track);
    setActiveStream(stream);
    syncStreamPosition();

    if(!isDecoding()) {
        start();
    }

    return stream;
}

DecodingController::DecodeResult DecodingController::decodeLoop()
{
    const auto loopStart = std::chrono::steady_clock::now();

    DecodeResult result{.stopDecodeTimer = true};

    if(!isValid() || !isDecoding()) {
        return result;
    }

    auto stream = activeStream();
    if(!stream) {
        return result;
    }

    const int effectiveLowWatermarkMs  = lowWatermarkMs();
    const int effectiveHighWatermarkMs = highWatermarkMs();
    const int reserveTargetMs          = clampMsToStreamCapacity(stream, std::max(0, m_reserveTargetMs), 0);
    const int fillTargetMs             = std::max(effectiveHighWatermarkMs, reserveTargetMs);
    const uint64_t bufferedMs          = stream->bufferedDurationMs();
    int burstChunkIterations{0};
    int burstDecodedChunks{0};
    bool burstCapReached{false};

    if(std::cmp_less(bufferedMs, effectiveLowWatermarkMs)
       || (reserveTargetMs > 0 && std::cmp_less(bufferedMs, reserveTargetMs))) {
        m_fillUntilTarget = true;
    }

    if(m_fillUntilTarget) {
        const int maxBurstChunks       = adaptiveBurstChunkLimit(stream, fillTargetMs);
        const size_t maxFramesPerChunk = decodeFramesPerChunk(stream);
        int chunkCount{0};

        while(chunkCount++ < maxBurstChunks) {
            ++burstChunkIterations;
            auto writer = stream->writer();
            if(writer.writeAvailable() == 0 || stream->endOfInput()) {
                break;
            }

            if(decodeChunk(maxFramesPerChunk) <= 0) {
                break;
            }
            ++burstDecodedChunks;

            if(stream->bufferedDurationMs() >= static_cast<uint64_t>(fillTargetMs)) {
                break;
            }
        }

        burstCapReached               = chunkCount > maxBurstChunks;
        uint64_t bufferedAfterBurstMs = stream->bufferedDurationMs();
        int shortfallMs               = (std::cmp_less(bufferedAfterBurstMs, static_cast<uint64_t>(fillTargetMs)))
                                          ? (fillTargetMs - static_cast<int>(bufferedAfterBurstMs))
                                          : 0;
        const int criticalFloorMs     = std::max(effectiveLowWatermarkMs, reserveTargetMs);
        bool belowCriticalFloor
            = std::cmp_less(bufferedAfterBurstMs, static_cast<uint64_t>(std::max(0, criticalFloorMs)));

        if(burstCapReached && !stream->endOfInput() && shortfallMs > 0 && belowCriticalFloor) {
            // Recovery pass: decode extra lookahead when fill-target bursts are
            // still below the critical floor
            const int recoveryHeadroomMs = std::max(effectiveLowWatermarkMs, BurstShortfallLogThresholdMs);
            const int recoveryTargetMs
                = clampMsToStreamCapacity(stream, fillTargetMs + recoveryHeadroomMs, criticalFloorMs);
            const int recoveryBurstChunks
                = std::max(BurstRecoveryMinChunks, adaptiveBurstChunkLimit(stream, recoveryTargetMs));
            int recoveryChunkCount{0};

            while(recoveryChunkCount++ < recoveryBurstChunks) {
                ++burstChunkIterations;

                auto writer = stream->writer();
                if(writer.writeAvailable() == 0 || stream->endOfInput()) {
                    break;
                }

                if(decodeChunk(maxFramesPerChunk) <= 0) {
                    break;
                }
                ++burstDecodedChunks;

                if(stream->bufferedDurationMs() >= static_cast<uint64_t>(recoveryTargetMs)) {
                    break;
                }
            }

            bufferedAfterBurstMs = stream->bufferedDurationMs();
            shortfallMs          = (std::cmp_less(bufferedAfterBurstMs, static_cast<uint64_t>(fillTargetMs)))
                                     ? (fillTargetMs - static_cast<int>(bufferedAfterBurstMs))
                                     : 0;
            belowCriticalFloor
                = std::cmp_less(bufferedAfterBurstMs, static_cast<uint64_t>(std::max(0, criticalFloorMs)));
        }

        if(burstCapReached && !stream->endOfInput() && shortfallMs >= BurstShortfallLogThresholdMs
           && belowCriticalFloor) {
            incrementAtomicSaturating(m_decodeBurstCapShortfallEvents);
            qCWarning(ENGINE) << "Decode burst reached cap before fill target:" << "buffered=" << bufferedAfterBurstMs
                              << "ms target=" << fillTargetMs << "ms floor=" << criticalFloorMs
                              << "ms chunks=" << maxBurstChunks
                              << "framesPerChunk=" << static_cast<qulonglong>(maxFramesPerChunk);
        }
    }

    const uint64_t bufferedAfterDecodeMs = stream->bufferedDurationMs();
    if(std::cmp_greater_equal(bufferedAfterDecodeMs, fillTargetMs) || stream->endOfInput()) {
        m_fillUntilTarget = false;
    }

    m_reserveTargetMs = reserveTargetMs;
    if(reserveTargetMs > 0 && std::cmp_greater_equal(bufferedAfterDecodeMs, reserveTargetMs)) {
        m_reserveTargetMs = 0;
    }

    const auto loopDurationMs
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - loopStart).count();
    if(loopDurationMs >= DecodeLoopDurationWarnMs) {
        qCWarning(ENGINE) << "Decode loop took too long:" << "durationMs=" << loopDurationMs
                          << "bufferedBeforeMs=" << bufferedMs << "bufferedAfterMs=" << bufferedAfterDecodeMs
                          << "lowMs=" << effectiveLowWatermarkMs << "highMs=" << effectiveHighWatermarkMs
                          << "reserveMs=" << reserveTargetMs << "fillTargetMs=" << fillTargetMs
                          << "burstIterations=" << burstChunkIterations << "burstDecodedChunks=" << burstDecodedChunks
                          << "burstCapReached=" << burstCapReached << "streamEndOfInput=" << stream->endOfInput();
    }

    result.stopDecodeTimer = !m_fillUntilTarget || stream->endOfInput() || stream->writer().writeAvailable() == 0;

    return result;
}

std::optional<DecodingController::DecodeResult> DecodingController::handleTimer(int timerId, bool seekInProgress)
{
    if(timerId != m_decodeTimerId || seekInProgress) {
        return {};
    }

    const auto now = std::chrono::steady_clock::now();
    if(m_hasLastDecodeTimerTick) {
        const auto gapMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastDecodeTimerTick).count();

        if(gapMs >= DecodeTimerGapWarnMs) {
            if(!m_decodeTimerGapLogActive) {
                m_decodeTimerGapLogActive = true;
                uint64_t bufferedMs{0};
                bool streamEndOfInput{false};

                if(const auto stream = activeStream()) {
                    bufferedMs       = stream->bufferedDurationMs();
                    streamEndOfInput = stream->endOfInput();
                }

                qCWarning(ENGINE) << "Decode timer gap detected:" << "gapMs=" << gapMs
                                  << "expectedMs=" << DecodeFillIntervalMs << "bufferedMs=" << bufferedMs
                                  << "streamEndOfInput=" << streamEndOfInput;
            }
        }
        else {
            m_decodeTimerGapLogActive = false;
        }
    }
    else {
        m_decodeTimerGapLogActive = false;
    }
    m_lastDecodeTimerTick    = now;
    m_hasLastDecodeTimerTick = true;

    return decodeLoop();
}
} // namespace Fooyin
