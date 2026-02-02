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

#include "nexttrackpreparer.h"

#include "decodercontext.h"
#include "enginehelpers.h"

#include <core/engine/audioloader.h>

#include <limits>

constexpr uint64_t PreparedStreamPrefillMs = 300;

namespace {
size_t bufferSamplesFromMs(uint64_t ms, int sampleRate, int channels)
{
    if(ms <= 0 || sampleRate <= 0 || channels <= 0) {
        return 0;
    }

    auto satMul = [](uint64_t a, uint64_t b) -> uint64_t {
        if(a == 0 || b == 0) {
            return 0;
        }
        if(a > (std::numeric_limits<uint64_t>::max() / b)) {
            return std::numeric_limits<uint64_t>::max();
        }
        return a * b;
    };

    const uint64_t msRate  = satMul(ms, static_cast<uint64_t>(sampleRate));
    const uint64_t rounded = (msRate > (std::numeric_limits<uint64_t>::max() - 999U))
                               ? std::numeric_limits<uint64_t>::max()
                               : (msRate + 999U);

    const uint64_t frames = rounded / 1000U;

    return static_cast<size_t>(satMul(frames, static_cast<uint64_t>(channels)));
}
} // namespace

namespace Fooyin {
NextTrackPreparationState NextTrackPreparer::prepare(const Track& track, const Context& context)
{
    NextTrackPreparationState state;
    state.track = track;

    const auto canceled = [&context]() {
        return context.cancelFlag && context.cancelFlag->load(std::memory_order_acquire);
    };

    if(!track.isValid() || !context.audioLoader || canceled()) {
        return {};
    }

    DecoderContext decoderContext;

    auto decoder = context.audioLoader->decoderForTrack(track);
    if(!decoder) {
        return {};
    }

    if(canceled()) {
        return {};
    }

    if(!decoderContext.init(std::move(decoder), track)) {
        return {};
    }

    if(canceled()) {
        return {};
    }

    state.format = decoderContext.format();

    const bool sameFileSegmentHandoff = isMultiTrackFileTransition(context.currentTrack, track);
    const bool canPrimePreparedStream = context.playbackState == Engine::PlaybackState::Playing
                                     && decoderContext.isSeekable() && !sameFileSegmentHandoff;

    if(canPrimePreparedStream) {
        const int channels   = state.format.channelCount();
        const int sampleRate = state.format.sampleRate();

        if(channels > 0 && sampleRate > 0 && context.bufferLengthMs > 0) {
            const size_t bufferSamples = bufferSamplesFromMs(context.bufferLengthMs, sampleRate, channels);
            auto preparedStream        = decoderContext.createStream(bufferSamples);
            decoderContext.setActiveStream(preparedStream);

            if(track.offset() > 0) {
                decoderContext.seek(track.offset());
            }

            if(canceled()) {
                return {};
            }

            decoderContext.start();

            const auto chunksDecoded
                = decoderContext.prefillActiveStreamMs(std::min(context.bufferLengthMs, PreparedStreamPrefillMs));

            if(canceled()) {
                return {};
            }

            if(chunksDecoded > 0 && preparedStream->bufferedSamples() > 0) {
                state.preparedStream           = decoderContext.detachStream();
                state.preparedDecodePositionMs = decoderContext.currentPosition();
            }
        }
    }

    state.decoder = decoderContext.takeDecoder();
    state.file    = decoderContext.takeFile();
    state.source  = decoderContext.takeSource();

    return state;
}

NextTrackPrepareWorker::NextTrackPrepareWorker()
    : m_nextJobToken{1}
    , m_activeJobToken{0} // 0 == idle
    , m_cancelFlag{std::make_shared<std::atomic<bool>>(false)}
{ }

NextTrackPrepareWorker::~NextTrackPrepareWorker()
{
    stop();
}

void NextTrackPrepareWorker::start(CompletionHandler handler)
{
    const std::scoped_lock lock{m_mutex};

    m_completion = std::move(handler);

    if(m_worker.joinable()) {
        return;
    }

    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);
    m_worker     = std::jthread{[this](std::stop_token stopToken) { run(stopToken); }};
}

void NextTrackPrepareWorker::stop()
{
    if(!m_worker.joinable()) {
        return;
    }

    {
        const std::scoped_lock lock{m_mutex};
        m_cancelFlag->store(true, std::memory_order_release);
        m_pendingRequest.reset();
        m_activeJobToken.store(0, std::memory_order_release);
    }

    m_worker.request_stop();
    m_cv.notify_all();

    m_worker.join();
}

void NextTrackPrepareWorker::cancelPendingJobs()
{
    const std::scoped_lock lock{m_mutex};

    m_cancelFlag->store(true, std::memory_order_release);
    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);
    m_pendingRequest.reset();
}

void NextTrackPrepareWorker::replacePending(Request request)
{
    {
        const std::scoped_lock lock{m_mutex};

        request.jobToken           = m_nextJobToken++;
        request.context.cancelFlag = m_cancelFlag;
        m_pendingRequest           = std::move(request);
    }

    m_cv.notify_one();
}

uint64_t NextTrackPrepareWorker::activeJobToken() const
{
    return m_activeJobToken.load(std::memory_order_acquire);
}

void NextTrackPrepareWorker::run(const std::stop_token& stopToken)
{
    while(true) {
        Request request;
        CompletionHandler completion;

        {
            std::unique_lock lock{m_mutex};
            m_cv.wait(lock, stopToken, [this]() { return m_pendingRequest.has_value(); });

            if(stopToken.stop_requested()) {
                return;
            }

            if(!m_pendingRequest.has_value()) {
                continue;
            }

            request = std::move(*m_pendingRequest);
            m_pendingRequest.reset();
            completion = m_completion;

            m_activeJobToken.store(request.jobToken, std::memory_order_release);
        }

        auto prepared = NextTrackPreparer::prepare(request.track, request.context);

        const bool canceled = request.context.cancelFlag && request.context.cancelFlag->load(std::memory_order_acquire);

        const uint64_t currentActive = m_activeJobToken.load(std::memory_order_acquire);
        const bool stale             = (currentActive != request.jobToken);

        if(!canceled && !stale && completion) {
            completion(request.jobToken, request.requestId, request.track, std::move(prepared));
        }
    }
}
} // namespace Fooyin
