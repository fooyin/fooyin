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

#include "outputfader.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Fooyin {
OutputFader::OutputFader()
    : m_fading{false}
    , m_fadingOut{false}
    , m_activeToken{0}
    , m_currentGain{1.0}
    , m_fadeStartGain{1.0}
    , m_targetVolume{1.0}
    , m_fadeFramesTotal{0}
    , m_fadeFramesDone{0}
    , m_curve{Engine::FadeCurve::Cosine}
{ }

void OutputFader::process(ProcessingBufferList& chunks)
{
    const size_t count = chunks.count();
    for(size_t i = 0; i < count; ++i) {
        auto* buffer = chunks.item(i);
        if(buffer) {
            processBuffer(*buffer);
        }
    }
}

void OutputFader::reset()
{
    stopFade();

    m_currentGain   = 1.0;
    m_fadeStartGain = 1.0;
    m_targetVolume  = 1.0;
    m_pendingCompletion.reset();
    m_activeToken = 0;
}

void OutputFader::fadeIn(int durationMs, double targetVolume, int sampleRate, uint64_t token)
{
    m_pendingCompletion.reset();
    m_activeToken = token;

    if(durationMs <= 0) {
        m_currentGain       = targetVolume;
        m_fadeStartGain     = targetVolume;
        m_targetVolume      = targetVolume;
        m_fadingOut         = false;
        m_fading            = false;
        m_fadeFramesTotal   = 0;
        m_fadeFramesDone    = 0;
        m_pendingCompletion = Completion{.type = CompletionType::FadeInComplete, .token = m_activeToken};
        return;
    }

    const bool wasFading = m_fading;
    double startGain     = m_currentGain;

    if(!wasFading) {
        startGain     = 0.0;
        m_currentGain = 0.0;
    }

    m_fadeStartGain = startGain;
    m_targetVolume  = targetVolume;
    m_fadingOut     = false;

    const int clampedSampleRate = (sampleRate > 0) ? sampleRate : 1;
    m_fadeFramesTotal
        = std::max<int64_t>(1, static_cast<int64_t>(durationMs) * static_cast<int64_t>(clampedSampleRate) / 1000);
    m_fadeFramesDone = 0;
    m_fading         = true;
}

void OutputFader::fadeOut(int durationMs, double currentVolume, int sampleRate, uint64_t token)
{
    m_pendingCompletion.reset();
    m_activeToken = token;

    if(durationMs <= 0) {
        m_currentGain       = 0.0;
        m_fadeStartGain     = 0.0;
        m_targetVolume      = 0.0;
        m_fading            = false;
        m_fadingOut         = false;
        m_fadeFramesTotal   = 0;
        m_fadeFramesDone    = 0;
        m_pendingCompletion = Completion{.type = CompletionType::FadeOutComplete, .token = m_activeToken};
        return;
    }

    // If a fade is already in progress (e.g. pause during resume fade-in), continue from the current gain
    const double startGain = std::clamp(m_fading ? m_currentGain : currentVolume, 0.0, 1.0);
    m_currentGain          = startGain;
    m_fadeStartGain        = startGain;
    m_targetVolume         = 0.0;
    m_fadingOut            = true;

    const int clampedSampleRate = (sampleRate > 0) ? sampleRate : 1;
    m_fadeFramesTotal
        = std::max<int64_t>(1, static_cast<int64_t>(durationMs) * static_cast<int64_t>(clampedSampleRate) / 1000);
    m_fadeFramesDone = 0;
    m_fading         = true;
}

void OutputFader::stopFade()
{
    m_fading          = false;
    m_fadingOut       = false;
    m_fadeFramesTotal = 0;
    m_fadeFramesDone  = 0;
    m_activeToken     = 0;
    m_pendingCompletion.reset();
}

bool OutputFader::isFading() const
{
    return m_fading;
}

std::optional<OutputFader::Completion> OutputFader::takeCompletion()
{
    return std::exchange(m_pendingCompletion, std::nullopt);
}

void OutputFader::setFadeCurve(Engine::FadeCurve curve)
{
    m_curve = curve;
}

void OutputFader::processBuffer(ProcessingBuffer& buffer)
{
    if(!buffer.isValid()) {
        return;
    }

    const int frames  = buffer.frameCount();
    const int samples = buffer.sampleCount();
    if(frames <= 0 || samples <= 0) {
        return;
    }

    const bool fading = m_fading;
    const double gain = m_currentGain;

    if(!fading && std::abs(gain - 1.0) < 0.0001) {
        return;
    }

    const int channels = buffer.format().channelCount();
    if(channels <= 0) {
        return;
    }

    auto dataSpan = buffer.data();
    if(std::cmp_less(dataSpan.size(), samples)) {
        return;
    }

    double* data = dataSpan.data();

    if(!fading) {
        for(int i{0}; i < samples; ++i) {
            data[i] *= gain;
        }
        return;
    }

    const int64_t totalFrames = m_fadeFramesTotal;
    if(totalFrames <= 0) {
        return;
    }

    int64_t doneFrames = m_fadeFramesDone;

    const double startGain                = m_fadeStartGain;
    const double targetGain               = m_targetVolume;
    const Engine::FadeCurve curve         = m_curve;
    const bool fadingOut                  = m_fadingOut;
    const Engine::FadeDirection direction = fadingOut ? Engine::FadeDirection::Out : Engine::FadeDirection::In;

    const double invTotal = 1.0 / static_cast<double>(totalFrames);
    double progress       = static_cast<double>(doneFrames) * invTotal;
    const double step     = invTotal;

    double lastGain = gain;

    double* framePtr = data;
    for(int frame{0}; frame < frames; ++frame, progress += step, framePtr += channels) {
        const double p          = std::clamp(progress, 0.0, 1.0);
        const double fadeGain01 = Engine::gain01(p, curve, direction);
        double frameGain{0.0};
        if(fadingOut) {
            frameGain = targetGain + ((startGain - targetGain) * fadeGain01);
        }
        else {
            frameGain = startGain + ((targetGain - startGain) * fadeGain01);
        }
        frameGain = std::clamp(frameGain, 0.0, 1.0);

        for(int ch{0}; ch < channels; ++ch) {
            framePtr[ch] *= frameGain;
        }

        lastGain = frameGain;
    }

    doneFrames += frames;
    m_fadeFramesDone = doneFrames;
    m_currentGain    = lastGain;

    if(doneFrames >= totalFrames) {
        m_fading = false;

        if(targetGain <= 0.0) {
            m_currentGain = 0.0;
        }

        const auto completionType = fadingOut ? CompletionType::FadeOutComplete : CompletionType::FadeInComplete;
        m_pendingCompletion       = Completion{.type = completionType, .token = m_activeToken};
        m_fadingOut               = false;
    }
}
} // namespace Fooyin
