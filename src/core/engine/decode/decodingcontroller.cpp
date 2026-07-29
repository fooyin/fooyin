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

#include <utils/compatutils.h>

#include <QLoggingCategory>
#include <QObject>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <limits>
#include <mutex>
#include <thread>

constexpr auto DecodeFillIntervalMs          = 20;
constexpr auto BaseDecodeFramesPerChunk      = 4096;
constexpr auto MaxDecodeFramesPerChunk       = 262144;
constexpr auto TargetChunkDurationMs         = 10;
constexpr auto BurstShortfallLogThresholdMs  = 200;
constexpr auto BurstRecoveryMinChunks        = 8;
constexpr auto DecodeTimerGapWarnMs          = 500;
constexpr auto DecodeLoopBudgetMs            = 100;
constexpr auto DecodeLoopDurationWarnMs      = 500;
constexpr auto DecodeHighWatermarkMaxRatio   = 0.99;
constexpr auto DecodeWatermarkMinHeadroomMs  = 20;
constexpr auto DecodeWatermarkMinGapMs       = 30;
constexpr auto SynchronousPrefillWaitTimeout = std::chrono::milliseconds{100};

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

    const auto frames = capacitySamples / static_cast<size_t>(channels);
    const uint64_t ms = (frames * 1000ULL) / static_cast<uint64_t>(rate);

    return static_cast<int>(std::min<uint64_t>(ms, std::numeric_limits<int>::max()));
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
    return std::min<uint64_t>(samples, std::numeric_limits<size_t>::max());
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
        = std::clamp<uint64_t>(desiredFrames, BaseDecodeFramesPerChunk, MaxDecodeFramesPerChunk);

    const size_t deficitSamples  = targetSamples - bufferedSamples;
    const size_t samplesPerChunk = framesPerChunk * static_cast<size_t>(channels);
    if(samplesPerChunk == 0) {
        return MinBurstChunks;
    }

    const uint64_t neededChunks
        = (static_cast<uint64_t>(deficitSamples) + static_cast<uint64_t>(samplesPerChunk) - 1ULL)
        / static_cast<uint64_t>(samplesPerChunk);

    return static_cast<int>(std::clamp<uint64_t>(neededChunks, MinBurstChunks, MaxBurstChunks));
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
    return std::clamp<uint64_t>(desiredFrames, BaseDecodeFramesPerChunk, MaxDecodeFramesPerChunk);
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
class DecodingControllerPrivate
{
public:
    using DecodeResult = DecodingController::DecodeResult;

    struct Snapshot
    {
        bool valid{false};
        bool decoding{false};
        bool seekable{false};
        AudioStreamPtr activeStream;
        StreamId activeStreamId{InvalidStreamId};
        Track track;
        AudioFormat format;
        uint64_t currentPosition{0};
        uint64_t startPosition{0};
        bool lastDecodeNeededMoreInput{false};
        AudioDecoder::PlaybackHints playbackHints{AudioDecoder::NoHints};
        DecoderContext::EndPolicy endPolicy{DecoderContext::EndPolicy::DecoderEofOnly};
        int bitrate{0};
        uint64_t metadataRevision{0};
    };

    using Command = MoveOnlyFunction<void()>;

    explicit DecodingControllerPrivate(QObject* host)
        : timerHost{host}
    {
        publishSnapshot(false);
        worker = std::jthread{[this](const std::stop_token& stopToken) { run(stopToken); }};
    }

    ~DecodingControllerPrivate()
    {
        requestPendingReadAbort();
        worker.request_stop();
        queueReady.notify_all();
        if(worker.joinable()) {
            worker.join();
        }
    }

    DecodingControllerPrivate(const DecodingControllerPrivate&)            = delete;
    DecodingControllerPrivate& operator=(const DecodingControllerPrivate&) = delete;

    template <typename Function>
    auto submit(Function&& function)
    {
        using Result = std::invoke_result_t<Function, DecoderContext&>;

        auto promise = std::make_shared<std::promise<Result>>();
        auto future  = promise->get_future();

        enqueue([this, promise, function = std::forward<Function>(function)]() mutable {
            try {
                if constexpr(std::is_void_v<Result>) {
                    std::invoke(function, context);
                    publishSnapshot();
                    promise->set_value();
                }
                else {
                    Result result = std::invoke(function, context);
                    publishSnapshot();
                    promise->set_value(std::move(result));
                }
            }
            catch(...) {
                promise->set_exception(std::current_exception());
            }
        });

        return future;
    }

    template <typename Function>
    auto invoke(Function&& function)
    {
        return submit(std::forward<Function>(function)).get();
    }

    template <typename Function>
    void post(Function&& function)
    {
        enqueue([this, function = std::forward<Function>(function)]() mutable {
            std::invoke(function, context);
            publishSnapshot();
        });
    }

    void enqueue(Command command)
    {
        {
            const std::scoped_lock lock{queueMutex};
            commands.emplace_back(std::move(command));
        }
        queueReady.notify_one();
    }

    void run(const std::stop_token& stopToken)
    {
        while(!stopToken.stop_requested()) {
            Command command;
            {
                std::unique_lock lock{queueMutex};
                queueReady.wait(lock, stopToken, [this]() { return !commands.empty(); });
                if(stopToken.stop_requested()) {
                    return;
                }
                if(commands.empty()) {
                    continue;
                }

                command = std::move(commands.front());
                commands.pop_front();
            }

            command();
        }
    }

    void publishSnapshot(bool refreshMetadata = true)
    {
        bool metadataChanged{false};
        if(refreshMetadata) {
            metadataChanged = context.refreshTrackMetadata();
        }

        Snapshot next;
        next.valid                     = context.isValid();
        next.decoding                  = context.isDecoding();
        next.seekable                  = context.isSeekable();
        next.activeStream              = context.activeStream();
        next.activeStreamId            = context.activeStreamId();
        next.track                     = context.track();
        next.format                    = context.format();
        next.currentPosition           = context.currentPosition();
        next.startPosition             = context.startPosition();
        next.lastDecodeNeededMoreInput = context.lastDecodeNeededMoreInput();
        next.playbackHints             = context.playbackHints();
        next.endPolicy                 = context.endPolicy();
        next.bitrate                   = context.bitrate();

        const std::scoped_lock lock{snapshotMutex};
        next.metadataRevision = snapshot.metadataRevision + (metadataChanged ? 1 : 0);
        snapshot              = std::move(next);
    }

    [[nodiscard]] Snapshot currentSnapshot() const
    {
        const std::scoped_lock lock{snapshotMutex};
        return snapshot;
    }

    [[nodiscard]] bool consumeMetadataChange()
    {
        const std::scoped_lock lock{snapshotMutex};
        if(consumedMetadataRevision == snapshot.metadataRevision) {
            return false;
        }
        consumedMetadataRevision = snapshot.metadataRevision;
        return true;
    }

    void setDecoder(AudioDecoder* currentDecoder)
    {
        decoder.store(currentDecoder, std::memory_order_release);
    }

    void requestPendingReadAbort() const
    {
        if(auto* currentDecoder = decoder.load(std::memory_order_acquire)) {
            currentDecoder->requestAbort();
        }
    }

    void scheduleDecode()
    {
        const uint64_t generation = decodeGen.load(std::memory_order_acquire);
        {
            const std::scoped_lock lock{decodeResultMutex};
            if(decodeJobPending || completedDecodeResult.has_value()) {
                return;
            }
            decodeJobPending = true;
        }

        enqueue([this, generation]() {
            const DecodeResult result = runDecodeLoop();
            publishSnapshot();

            const std::scoped_lock lock{decodeResultMutex};
            if(generation == decodeGen.load(std::memory_order_acquire)) {
                completedDecodeResult = result;
            }
            decodeJobPending = false;
        });
    }

    [[nodiscard]] std::optional<DecodingController::DecodeResult> takeDecodeResult()
    {
        const std::scoped_lock lock{decodeResultMutex};
        return std::exchange(completedDecodeResult, {});
    }

    void invalidateDecodeJobs()
    {
        decodeGen.fetch_add(1, std::memory_order_acq_rel);
        const std::scoped_lock lock{decodeResultMutex};
        completedDecodeResult.reset();
    }

    [[nodiscard]] int effectiveLowWatermarkMs() const
    {
        const auto current = currentSnapshot();
        return effectiveWatermarksForStream(current.activeStream, lowWatermarkMs.load(std::memory_order_relaxed),
                                            highWatermarkMs.load(std::memory_order_relaxed))
            .first;
    }

    [[nodiscard]] int effectiveHighWatermarkMs() const
    {
        const auto current = currentSnapshot();
        return effectiveWatermarksForStream(current.activeStream, lowWatermarkMs.load(std::memory_order_relaxed),
                                            highWatermarkMs.load(std::memory_order_relaxed))
            .second;
    }

    DecodeResult runDecodeLoop()
    {
        const auto loopStart = std::chrono::steady_clock::now();

        DecodeResult result;
        result.stopDecodeTimer = true;
        inputNeedsMoreData.store(false, std::memory_order_relaxed);

        if(!context.isValid() || !context.isDecoding()) {
            return result;
        }

        auto stream = context.activeStream();
        if(!stream) {
            return result;
        }

        const auto effectiveWatermarks = effectiveWatermarksForStream(
            stream, lowWatermarkMs.load(std::memory_order_relaxed), highWatermarkMs.load(std::memory_order_relaxed));
        const int effectiveLowMs     = effectiveWatermarks.first;
        const int effectiveHighMs    = effectiveWatermarks.second;
        const int requestedReserveMs = std::max(0, reserveTargetMs.load(std::memory_order_relaxed));
        const int reserveMs          = clampMsToStreamCapacity(stream, requestedReserveMs, 0);
        const int fillTargetMs       = std::max(effectiveHighMs, reserveMs);
        const uint64_t bufferedMs    = stream->bufferedDurationMs();
        int burstChunkIterations{0};
        int burstDecodedChunks{0};
        bool burstCapReached{false};
        bool decodeBudgetReached{false};

        const auto reachedDecodeBudget = [&loopStart]() {
            const auto elapsedMs
                = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - loopStart)
                      .count();
            return elapsedMs >= DecodeLoopBudgetMs;
        };

        if(std::cmp_less(bufferedMs, effectiveLowMs) || (reserveMs > 0 && std::cmp_less(bufferedMs, reserveMs))) {
            fillUntilTarget.store(true, std::memory_order_relaxed);
        }

        if(fillUntilTarget.load(std::memory_order_relaxed)) {
            const int maxBurstChunks       = adaptiveBurstChunkLimit(stream, fillTargetMs);
            const size_t maxFramesPerChunk = decodeFramesPerChunk(stream);
            int chunkCount{0};

            while(chunkCount++ < maxBurstChunks) {
                if(reachedDecodeBudget()) {
                    decodeBudgetReached = true;
                    break;
                }

                ++burstChunkIterations;
                auto writer = stream->writer();
                if(writer.writeAvailable() == 0 || stream->endOfInput()) {
                    break;
                }

                if(context.decodeChunk(maxFramesPerChunk) <= 0) {
                    if(context.lastDecodeNeededMoreInput()) {
                        result.inputNeedsMoreData = true;
                        inputNeedsMoreData.store(true, std::memory_order_relaxed);
                    }
                    break;
                }
                ++burstDecodedChunks;

                if(stream->bufferedDurationMs() >= static_cast<uint64_t>(fillTargetMs)) {
                    break;
                }
            }

            burstCapReached               = chunkCount > maxBurstChunks;
            uint64_t bufferedAfterBurstMs = stream->bufferedDurationMs();
            int shortfallMs               = std::cmp_less(bufferedAfterBurstMs, static_cast<uint64_t>(fillTargetMs))
                                              ? fillTargetMs - static_cast<int>(bufferedAfterBurstMs)
                                              : 0;
            const int criticalFloorMs     = std::max(effectiveLowMs, reserveMs);
            bool belowCriticalFloor
                = std::cmp_less(bufferedAfterBurstMs, static_cast<uint64_t>(std::max(0, criticalFloorMs)));

            if(burstCapReached && !stream->endOfInput() && shortfallMs > 0 && belowCriticalFloor) {
                const int recoveryHeadroomMs = std::max(effectiveLowMs, BurstShortfallLogThresholdMs);
                const int recoveryTargetMs
                    = clampMsToStreamCapacity(stream, fillTargetMs + recoveryHeadroomMs, criticalFloorMs);
                const int recoveryBurstChunks
                    = std::max(BurstRecoveryMinChunks, adaptiveBurstChunkLimit(stream, recoveryTargetMs));
                int recoveryChunkCount{0};

                while(recoveryChunkCount++ < recoveryBurstChunks) {
                    if(reachedDecodeBudget()) {
                        decodeBudgetReached = true;
                        break;
                    }

                    ++burstChunkIterations;
                    auto writer = stream->writer();
                    if(writer.writeAvailable() == 0 || stream->endOfInput()) {
                        break;
                    }

                    if(context.decodeChunk(maxFramesPerChunk) <= 0) {
                        if(context.lastDecodeNeededMoreInput()) {
                            result.inputNeedsMoreData = true;
                            inputNeedsMoreData.store(true, std::memory_order_relaxed);
                        }
                        break;
                    }
                    ++burstDecodedChunks;

                    if(stream->bufferedDurationMs() >= static_cast<uint64_t>(recoveryTargetMs)) {
                        break;
                    }
                }

                bufferedAfterBurstMs = stream->bufferedDurationMs();
                shortfallMs          = std::cmp_less(bufferedAfterBurstMs, static_cast<uint64_t>(fillTargetMs))
                                         ? fillTargetMs - static_cast<int>(bufferedAfterBurstMs)
                                         : 0;
                belowCriticalFloor
                    = std::cmp_less(bufferedAfterBurstMs, static_cast<uint64_t>(std::max(0, criticalFloorMs)));
            }

            if(burstCapReached && !stream->endOfInput() && shortfallMs >= BurstShortfallLogThresholdMs
               && belowCriticalFloor && !decodeBudgetReached) {
                incrementAtomicSaturating(decodeBurstCapShortfallEvents);
                qCWarning(ENGINE) << "Decode burst reached cap before fill target:"
                                  << "buffered=" << bufferedAfterBurstMs << "ms target=" << fillTargetMs
                                  << "ms floor=" << criticalFloorMs << "ms chunks=" << maxBurstChunks
                                  << "framesPerChunk=" << static_cast<qulonglong>(maxFramesPerChunk);
            }
        }

        const uint64_t bufferedAfterDecodeMs = stream->bufferedDurationMs();
        if(std::cmp_greater_equal(bufferedAfterDecodeMs, fillTargetMs) || stream->endOfInput()) {
            fillUntilTarget.store(false, std::memory_order_relaxed);
        }

        reserveTargetMs.store(reserveMs, std::memory_order_relaxed);
        if(reserveMs > 0 && std::cmp_greater_equal(bufferedAfterDecodeMs, reserveMs)) {
            reserveTargetMs.store(0, std::memory_order_relaxed);
        }

        const auto loopDurationMs
            = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - loopStart)
                  .count();
        if(loopDurationMs >= DecodeLoopDurationWarnMs) {
            qCWarning(ENGINE) << "Decode loop took too long:"
                              << "durationMs=" << loopDurationMs << "bufferedBeforeMs=" << bufferedMs
                              << "bufferedAfterMs=" << bufferedAfterDecodeMs << "lowMs=" << effectiveLowMs
                              << "highMs=" << effectiveHighMs << "reserveMs=" << reserveMs
                              << "fillTargetMs=" << fillTargetMs << "burstIterations=" << burstChunkIterations
                              << "burstDecodedChunks=" << burstDecodedChunks << "burstCapReached=" << burstCapReached
                              << "decodeBudgetReached=" << decodeBudgetReached
                              << "streamEndOfInput=" << stream->endOfInput();
        }

        result.stopDecodeTimer = !fillUntilTarget.load(std::memory_order_relaxed) || stream->endOfInput()
                              || stream->writer().writeAvailable() == 0;
        return result;
    }

    DecoderContext context;
    QObject* timerHost{nullptr};

    mutable std::mutex queueMutex;
    std::condition_variable_any queueReady;
    std::deque<Command> commands;
    std::jthread worker;

    mutable std::mutex snapshotMutex;
    Snapshot snapshot;
    uint64_t consumedMetadataRevision{0};

    mutable std::mutex decodeResultMutex;
    std::optional<DecodeResult> completedDecodeResult;
    bool decodeJobPending{false};
    std::atomic<uint64_t> decodeGen{1};

    std::atomic<AudioDecoder*> decoder{nullptr};
    std::atomic<int> lowWatermarkMs{200};
    std::atomic<int> highWatermarkMs{400};
    std::atomic<int> reserveTargetMs{0};
    std::atomic<bool> fillUntilTarget{false};
    std::atomic<bool> inputNeedsMoreData{false};
    std::atomic<uint64_t> decodeReserveClampEvents{0};
    std::atomic<uint64_t> decodeBurstCapShortfallEvents{0};

    int decodeTimerId{0};
    std::chrono::steady_clock::time_point lastDecodeTimerTick;
    bool hasLastDecodeTimerTick{false};
    bool decodeTimerGapLogActive{false};
};

DecodingController::DecodingController(QObject* timerHost)
    : p{std::make_unique<DecodingControllerPrivate>(timerHost)}
{ }

DecodingController::~DecodingController()
{
    stopDecodeTimer();
}

void DecodingController::startDecoding()
{
    if(!isDecoding()) {
        start();
    }

    p->fillUntilTarget.store(true, std::memory_order_relaxed);
    p->inputNeedsMoreData.store(false, std::memory_order_relaxed);
    p->hasLastDecodeTimerTick  = false;
    p->decodeTimerGapLogActive = false;

    ensureDecodeTimerRunning();
}

void DecodingController::stopDecoding()
{
    stopDecodeTimer();
    stop();

    p->fillUntilTarget.store(false, std::memory_order_relaxed);
    p->reserveTargetMs.store(0, std::memory_order_relaxed);
    p->inputNeedsMoreData.store(false, std::memory_order_relaxed);
    p->hasLastDecodeTimerTick  = false;
    p->decodeTimerGapLogActive = false;
}

void DecodingController::requestAbort()
{
    p->requestPendingReadAbort();
}

void DecodingController::stopDecodeTimer()
{
    if(p->decodeTimerId != 0 && p->timerHost) {
        p->timerHost->killTimer(p->decodeTimerId);
        p->decodeTimerId = 0;
    }

    p->hasLastDecodeTimerTick  = false;
    p->decodeTimerGapLogActive = false;

    p->invalidateDecodeJobs();
}

void DecodingController::ensureDecodeTimerRunning()
{
    if(p->decodeTimerId == 0 && p->timerHost) {
        p->decodeTimerId = p->timerHost->startTimer(DecodeFillIntervalMs, Qt::CoarseTimer);
    }
}

bool DecodingController::isDecodeTimerActive() const
{
    return p->decodeTimerId != 0;
}

bool DecodingController::inputNeedsMoreData() const
{
    return p->inputNeedsMoreData.load(std::memory_order_relaxed);
}

void DecodingController::setBufferWatermarksMs(int lowWatermarkMs, int highWatermarkMs)
{
    const int low  = std::max(1, lowWatermarkMs);
    const int high = std::max(low, highWatermarkMs);

    p->lowWatermarkMs.store(low, std::memory_order_relaxed);
    p->highWatermarkMs.store(high, std::memory_order_relaxed);

    int reserve = p->reserveTargetMs.load(std::memory_order_relaxed);
    if(reserve > 0) {
        reserve = std::max(reserve, high);
        reserve = clampMsToStreamCapacity(activeStream(), reserve, 0);
        p->reserveTargetMs.store(reserve, std::memory_order_relaxed);
    }
}

void DecodingController::requestDecodeReserveMs(int reserveMs)
{
    const int requested        = std::max(0, reserveMs);
    const int clampedRequested = clampMsToStreamCapacity(activeStream(), requested, 0);

    int current = p->reserveTargetMs.load(std::memory_order_relaxed);
    while(current < clampedRequested
          && !p->reserveTargetMs.compare_exchange_weak(current, clampedRequested, std::memory_order_relaxed,
                                                       std::memory_order_relaxed)) { }

    if(requested > 0 && clampedRequested < requested) {
        incrementAtomicSaturating(p->decodeReserveClampEvents);
        qCWarning(ENGINE) << "Decode reserve clamped to stream capacity:" << clampedRequested
                          << "ms requested:" << requested << "ms";
    }
}

void DecodingController::clearDecodeReserve()
{
    p->reserveTargetMs.store(0, std::memory_order_relaxed);
}

int DecodingController::lowWatermarkMs() const
{
    return p->effectiveLowWatermarkMs();
}

int DecodingController::highWatermarkMs() const
{
    return p->effectiveHighWatermarkMs();
}

bool DecodingController::isValid() const
{
    return p->currentSnapshot().valid;
}

bool DecodingController::isDecoding() const
{
    return p->currentSnapshot().decoding;
}

bool DecodingController::isSeekable() const
{
    return p->currentSnapshot().seekable;
}

AudioStreamPtr DecodingController::activeStream() const
{
    return p->currentSnapshot().activeStream;
}

StreamId DecodingController::activeStreamId() const
{
    return p->currentSnapshot().activeStreamId;
}

Track DecodingController::track() const
{
    return p->currentSnapshot().track;
}

AudioFormat DecodingController::format() const
{
    return p->currentSnapshot().format;
}

uint64_t DecodingController::currentPosition() const
{
    return p->currentSnapshot().currentPosition;
}

uint64_t DecodingController::startPosition() const
{
    return p->currentSnapshot().startPosition;
}

bool DecodingController::lastDecodeNeededMoreInput() const
{
    return p->currentSnapshot().lastDecodeNeededMoreInput;
}

AudioDecoder::PlaybackHints DecodingController::playbackHints() const
{
    return p->currentSnapshot().playbackHints;
}

void DecodingController::setPlaybackHints(AudioDecoder::PlaybackHints hints)
{
    p->post([hints](DecoderContext& context) { context.setPlaybackHints(hints); });
}

bool DecodingController::init(LoadedDecoder decoder, const Track& track)
{
    const bool result = p->invoke([this, decoderHandle = decoder.decoder.get(), decoder = std::move(decoder),
                                   track](DecoderContext& context) mutable {
        p->setDecoder(decoderHandle);
        return context.init(std::move(decoder), track);
    });
    if(!result) {
        p->setDecoder(nullptr);
    }
    return result;
}

bool DecodingController::adoptPreparedDecoder(LoadedDecoder decoder, const Track& track)
{
    const bool result = p->invoke([this, decoderHandle = decoder.decoder.get(), decoder = std::move(decoder),
                                   track](DecoderContext& context) mutable {
        p->setDecoder(decoderHandle);
        return context.adoptPreparedDecoder(std::move(decoder), track);
    });
    if(!result) {
        p->setDecoder(nullptr);
    }
    return result;
}

void DecodingController::setPreparedDecodePosition(uint64_t positionMs)
{
    p->invoke([positionMs](DecoderContext& context) { context.setPreparedDecodePosition(positionMs); });
}

AudioStreamPtr DecodingController::createStream(size_t bufferSamples, Engine::FadeCurve fadeCurve)
{
    return p->invoke(
        [bufferSamples, fadeCurve](DecoderContext& context) { return context.createStream(bufferSamples, fadeCurve); });
}

void DecodingController::setActiveStream(AudioStreamPtr stream)
{
    p->invoke(
        [stream = std::move(stream)](DecoderContext& context) mutable { context.setActiveStream(std::move(stream)); });
}

AudioStreamPtr DecodingController::detachStream()
{
    return p->invoke([](DecoderContext& context) { return context.detachStream(); });
}

void DecodingController::start()
{
    p->invoke([](DecoderContext& context) { context.start(); });
}

void DecodingController::stop()
{
    p->requestPendingReadAbort();
    p->invoke([](DecoderContext& context) { context.stop(); });
    p->setDecoder(nullptr);
}

bool DecodingController::seek(uint64_t positionMs)
{
    if(!isSeekable()) {
        return false;
    }
    return p->invoke([positionMs](DecoderContext& context) { return context.seek(positionMs); });
}

bool DecodingController::switchContiguousTrack(const Track& track)
{
    return p->invoke([track](DecoderContext& context) { return context.switchContiguousTrack(track); });
}

void DecodingController::setEndPolicy(DecoderContext::EndPolicy policy, std::optional<uint64_t> windowEndMs)
{
    p->post([policy, windowEndMs](DecoderContext& context) { context.setEndPolicy(policy, windowEndMs); });
}

DecoderContext::EndPolicy DecodingController::endPolicy() const
{
    return p->currentSnapshot().endPolicy;
}

void DecodingController::syncStreamPosition()
{
    p->invoke([](DecoderContext& context) { context.syncStreamPosition(); });
}

int DecodingController::prefillActiveStream(size_t targetSamples, int maxChunks, size_t maxFramesPerChunk)
{
    auto future = p->submit([targetSamples, maxChunks, maxFramesPerChunk](DecoderContext& context) {
        return context.prefillActiveStream(targetSamples, maxChunks, maxFramesPerChunk);
    });
    if(future.wait_for(SynchronousPrefillWaitTimeout) != std::future_status::ready) {
        return 0;
    }
    return future.get();
}

int DecodingController::prefillActiveStreamMs(uint64_t targetMs, int maxChunks, size_t maxFramesPerChunk)
{
    auto future = p->submit([targetMs, maxChunks, maxFramesPerChunk](DecoderContext& context) {
        return context.prefillActiveStreamMs(targetMs, maxChunks, maxFramesPerChunk);
    });
    if(future.wait_for(SynchronousPrefillWaitTimeout) != std::future_status::ready) {
        return 0;
    }
    return future.get();
}

bool DecodingController::refreshTrackMetadata()
{
    return p->consumeMetadataChange();
}

int DecodingController::bitrate() const
{
    return p->currentSnapshot().bitrate;
}

LoadedDecoder DecodingController::takeLoadedDecoder()
{
    p->requestPendingReadAbort();
    LoadedDecoder decoder = p->invoke([](DecoderContext& context) { return context.takeLoadedDecoder(); });
    p->setDecoder(nullptr);
    return decoder;
}

void DecodingController::reset()
{
    p->invalidateDecodeJobs();
    p->requestPendingReadAbort();
    p->invoke([](DecoderContext& context) { context.reset(); });
    p->setDecoder(nullptr);
}

AudioStreamPtr DecodingController::setupCrossfadeStream(int bufferLengthMs, Engine::FadeCurve curve)
{
    return p->invoke([bufferLengthMs, curve](DecoderContext& context) {
        auto stream = context.activeStream();
        if(!stream) {
            const AudioFormat& decoderFormat = context.format();
            const size_t bufferSamples
                = Audio::bufferSamplesFromMs(bufferLengthMs, decoderFormat.sampleRate(), decoderFormat.channelCount());
            stream = context.createStream(bufferSamples, curve);
            if(!stream) {
                return AudioStreamPtr{};
            }

            stream->setTrack(context.track());
            context.setActiveStream(stream);

            if(context.startPosition() > 0) {
                context.seek(context.startPosition());
                context.syncStreamPosition();
            }
        }
        else {
            stream->setTrack(context.track());
            stream->setFadeCurve(curve);
        }

        if(!context.isDecoding()) {
            context.start();
        }
        return stream;
    });
}

AudioStreamPtr DecodingController::prepareSeekStream(uint64_t seekPosMs, int bufferLengthMs, Engine::FadeCurve curve,
                                                     const Track& track)
{
    clearDecodeReserve();

    return p->invoke([seekPosMs, bufferLengthMs, curve, track](DecoderContext& context) {
        context.seek(seekPosMs);

        const AudioFormat& decoderFormat = context.format();
        const size_t bufferSamples
            = Audio::bufferSamplesFromMs(bufferLengthMs, decoderFormat.sampleRate(), decoderFormat.channelCount());
        auto stream = context.createStream(bufferSamples, curve);
        if(!stream) {
            return AudioStreamPtr{};
        }

        stream->setTrack(track);
        context.setActiveStream(stream);
        context.syncStreamPosition();

        if(!context.isDecoding()) {
            context.start();
        }
        return stream;
    });
}

DecodingController::DecodeResult DecodingController::decodeLoop()
{
    return p->invoke([this](DecoderContext&) { return p->runDecodeLoop(); });
}

std::optional<DecodingController::DecodeResult> DecodingController::handleTimer(int timerId, bool seekInProgress)
{
    if(timerId != p->decodeTimerId || seekInProgress) {
        return {};
    }

    const auto now = std::chrono::steady_clock::now();
    if(p->hasLastDecodeTimerTick) {
        const auto gapMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - p->lastDecodeTimerTick).count();

        if(gapMs >= DecodeTimerGapWarnMs) {
            if(!p->decodeTimerGapLogActive) {
                p->decodeTimerGapLogActive = true;
                const auto stream          = activeStream();
                const uint64_t bufferedMs  = stream ? stream->bufferedDurationMs() : 0;
                const bool endOfInput      = stream && stream->endOfInput();

                qCWarning(ENGINE) << "Decode timer gap detected:"
                                  << "gapMs=" << gapMs << "expectedMs=" << DecodeFillIntervalMs
                                  << "bufferedMs=" << bufferedMs << "streamEndOfInput=" << endOfInput;
            }
        }
        else {
            p->decodeTimerGapLogActive = false;
        }
    }
    else {
        p->decodeTimerGapLogActive = false;
    }
    p->lastDecodeTimerTick    = now;
    p->hasLastDecodeTimerTick = true;

    if(auto result = p->takeDecodeResult()) {
        if(!result->stopDecodeTimer) {
            p->scheduleDecode();
        }
        return result;
    }

    p->scheduleDecode();
    return {};
}
} // namespace Fooyin
