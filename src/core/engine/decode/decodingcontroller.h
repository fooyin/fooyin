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

#include "fycore_export.h"

#include "decodercontext.h"

#include <core/engine/enginedefs.h>

#include <optional>

class QObject;

namespace Fooyin {
/*!
 * Timer-driven decode loop wrapper around DecoderContext.
 *
 * DecodingController owns the periodic decode timer used by AudioEngine and
 * exposes structured decode-loop results used to trigger follow-up actions
 * (pending seek checks, orphan stream cleanup).
 */
class FYCORE_EXPORT DecodingController : public DecoderContext
{
public:
    struct DecodeResult
    {
        bool stopDecodeTimer{false};
    };

    explicit DecodingController(QObject* timerHost);
    ~DecodingController() = default;

    DecodingController(const DecodingController&)            = delete;
    DecodingController& operator=(const DecodingController&) = delete;
    DecodingController(DecodingController&&)                 = default;
    DecodingController& operator=(DecodingController&&)      = default;

    //! Start decoding and arm periodic decode timer if needed.
    void startDecoding();
    //! Stop decode timer and stop decode state.
    void stopDecoding();
    //! Stop decode timer only (keep decoder context intact).
    void stopDecodeTimer();
    //! Ensure decode timer is active.
    void ensureDecodeTimerRunning();
    [[nodiscard]] bool isDecodeTimerActive() const;
    //! Configure decode hysteresis watermarks (milliseconds).
    void setBufferWatermarksMs(int lowWatermarkMs, int highWatermarkMs);
    //! Temporary reserve target for burst operations (for e.g. seek fade-out).
    void requestDecodeReserveMs(int reserveMs);
    //! Clear temporary reserve target.
    void clearDecodeReserve();
    [[nodiscard]] int lowWatermarkMs() const;
    [[nodiscard]] int highWatermarkMs() const;

    //! Prepare a stream for an incoming crossfade track using current decoder context.
    //! Assumes decoder is already initialised for the new track.
    [[nodiscard]] AudioStreamPtr setupCrossfadeStream(int bufferLengthMs, Engine::FadeCurve curve);
    //! Prepare a stream for seek crossfade at the given absolute position.
    [[nodiscard]] AudioStreamPtr prepareSeekStream(uint64_t seekPosMs, int bufferLengthMs, Engine::FadeCurve curve,
                                                   const Track& track);

    //! Perform one decode iteration and return requested follow-up actions.
    [[nodiscard]] DecodeResult decodeLoop();
    //! Handle Qt timer event id; returns nullopt for unrelated timer ids.
    [[nodiscard]] std::optional<DecodeResult> handleTimer(int timerId, bool seekInProgress);

private:
    QObject* m_timerHost;

    int m_decodeTimerId;
    int m_lowWatermarkMs;
    int m_highWatermarkMs;
    int m_reserveTargetMs;
    bool m_fillUntilTarget;
};
} // namespace Fooyin
