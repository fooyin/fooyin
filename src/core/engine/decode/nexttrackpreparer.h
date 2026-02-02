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

#pragma once

#include "core/engine/pipeline/audiostream.h"

#include <core/engine/audioformat.h>
#include <core/engine/audioinput.h>
#include <core/engine/enginedefs.h>
#include <core/track.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

class QFile;

namespace Fooyin {
class AudioLoader;

struct NextTrackPreparationState
{
    Track track;
    std::unique_ptr<AudioDecoder> decoder;
    std::unique_ptr<QFile> file;
    AudioSource source;
    AudioFormat format;
    AudioStreamPtr preparedStream;
    uint64_t preparedDecodePositionMs{0};

    [[nodiscard]] bool isValid() const
    {
        return track.isValid() && decoder != nullptr && format.isValid();
    }
};

/*!
 * Helper that prepares next-track decode state.
 *
 * Called by `NextTrackPrepareWorker` to pre-open decoder resources and
 * optionally pre-create/prefill a stream for transition handoff.
 */
class NextTrackPreparer
{
public:
    struct Context
    {
        std::shared_ptr<AudioLoader> audioLoader;
        Track currentTrack; // Used to check for same-file transition
        Engine::PlaybackState playbackState{Engine::PlaybackState::Stopped};
        uint64_t bufferLengthMs{0}; // Preferred stream buffer target for prefill
        std::shared_ptr<std::atomic<bool>> cancelFlag;
    };

    /*!
     * Prepare decoder and optional prefilled stream for @p track.
     *
     * Ownership of returned resources is
     * transferred to caller via NextTrackPreparationState.
     */
    [[nodiscard]] static NextTrackPreparationState prepare(const Track& track, const Context& context);
};

/*!
 * Single-worker background queue for next-track preparation jobs.
 *
 * Queue semantics are replace-latest: at most one pending request is retained.
 * Completion callback is invoked from the worker thread.
 */
class NextTrackPrepareWorker
{
public:
    struct Request
    {
        uint64_t jobToken{0};
        uint64_t requestId{0};
        Track track;
        NextTrackPreparer::Context context;
    };

    using CompletionHandler = std::function<void(uint64_t jobToken, uint64_t requestId, const Track& track,
                                                 NextTrackPreparationState prepared)>;

    NextTrackPrepareWorker();
    ~NextTrackPrepareWorker();

    NextTrackPrepareWorker(const NextTrackPrepareWorker&)            = delete;
    NextTrackPrepareWorker& operator=(const NextTrackPrepareWorker&) = delete;

    //! Start worker thread and set completion callback.
    void start(CompletionHandler handler);
    //! Stop worker and clear pending request.
    void stop();
    //! Cancel active/pending preparation without stopping worker thread.
    void cancelPendingJobs();
    //! Single-slot queue semantics: replaces any previously pending request.
    void replacePending(Request request);

    [[nodiscard]] uint64_t activeJobToken() const;

private:
    void run(const std::stop_token& stopToken);

    mutable std::mutex m_mutex;
    std::condition_variable_any m_cv;
    std::optional<Request> m_pendingRequest;
    CompletionHandler m_completion;
    std::jthread m_worker;
    uint64_t m_nextJobToken;
    std::atomic<uint64_t> m_activeJobToken;
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
};
} // namespace Fooyin
