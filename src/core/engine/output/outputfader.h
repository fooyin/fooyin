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

#include <core/engine/dsp/processingbufferlist.h>
#include <core/engine/enginedefs.h>

#include <cstdint>
#include <optional>

namespace Fooyin {
/*!
 * Final-stage output fade for transport transitions (play/pause/stop/manual change).
 * Owned and processed on the audio thread.
 */
class FYCORE_EXPORT OutputFader
{
public:
    OutputFader();

    enum class CompletionType : uint8_t
    {
        FadeInComplete,
        FadeOutComplete,
    };

    struct Completion
    {
        CompletionType type{CompletionType::FadeInComplete};
        uint64_t token{0};
    };

    void process(ProcessingBufferList& chunks);

    void reset();
    void fadeIn(int durationMs, double targetVolume, int sampleRate, uint64_t token = 0);
    void fadeOut(int durationMs, double currentVolume, int sampleRate, uint64_t token = 0);
    void stopFade();

    [[nodiscard]] bool isFading() const;
    [[nodiscard]] std::optional<Completion> takeCompletion();

    void setFadeCurve(Engine::FadeCurve curve);

private:
    void processBuffer(ProcessingBuffer& buffer);

    bool m_fading;
    bool m_fadingOut;
    uint64_t m_activeToken;
    std::optional<Completion> m_pendingCompletion;
    double m_currentGain;
    double m_fadeStartGain;
    double m_targetVolume;
    int64_t m_fadeFramesTotal;
    int64_t m_fadeFramesDone;
    Engine::FadeCurve m_curve;
};
} // namespace Fooyin
