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

#include <core/engine/visualisationservice.h>

#include "dsp/resamplerdsp.h"
#include "visualisationbackend.h"

#include <utils/timeconstants.h>

#include <QPointer>

#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <ranges>
#include <utility>

namespace Fooyin {
class PcmWindowResampler
{
public:
    void setTargetRate(int targetRate)
    {
        targetRate = std::max(0, targetRate);
        if(m_targetRate != targetRate) {
            m_targetRate        = targetRate;
            m_inputFormat       = {};
            m_haveExpectedStart = false;
            if(m_targetRate > 0) {
                static_cast<void>(m_resampler.setTargetSampleRate(m_targetRate));
            }
            m_resampler.reset();
        }
    }

    bool process(VisualisationSession::PcmWindow& window)
    {
        if(m_targetRate <= 0 || !window.isValid() || window.format.sampleRate() == m_targetRate) {
            return true;
        }

        AudioFormat inputFormat{window.format};
        inputFormat.setSampleFormat(SampleFormat::F64);
        const int channels = inputFormat.channelCount();
        if(inputFormat.sampleRate() <= 0 || channels <= 0) {
            return false;
        }

        const uint64_t inputEndMs = window.startTimeMs + window.format.durationForFrames(window.frameCount);
        const bool discontinuity
            = m_haveExpectedStart
           && std::llabs(static_cast<int64_t>(window.startTimeMs) - static_cast<int64_t>(m_expectedStartMs)) > 2;
        if(discontinuity || inputFormat != m_inputFormat) {
            m_inputFormat = inputFormat;
            static_cast<void>(m_resampler.setTargetSampleRate(m_targetRate));
            m_resampler.prepare(m_inputFormat);
            m_haveExpectedStart = false;
        }

        ProcessingBufferList chunks;
        auto* input = chunks.addItem(m_inputFormat, window.startTimeMs * Time::NsPerMs, window.sampleCount());
        if(!input) {
            return false;
        }

        std::ranges::transform(window.interleavedSamples(), input->data().begin(),
                               [](float sample) { return static_cast<double>(sample); });
        m_resampler.process(chunks);

        size_t outputSamples{0};
        AudioFormat outputFormat;
        for(size_t i{0}; i < chunks.count(); ++i) {
            const auto* chunk = chunks.item(i);
            if(!chunk || !chunk->isValid() || chunk->format().sampleRate() != m_targetRate
               || chunk->format().channelCount() != channels) {
                continue;
            }
            if(!outputFormat.isValid()) {
                outputFormat = chunk->format();
            }
            outputSamples += static_cast<size_t>(chunk->sampleCount());
        }

        std::vector<float> converted;
        converted.reserve(outputSamples);
        for(size_t i{0}; i < chunks.count(); ++i) {
            const auto* chunk = chunks.item(i);
            if(!chunk || !chunk->isValid() || chunk->format().sampleRate() != m_targetRate
               || chunk->format().channelCount() != channels) {
                continue;
            }
            std::ranges::transform(chunk->constData(), std::back_inserter(converted),
                                   [](const double sample) { return static_cast<float>(sample); });
        }

        const int outputFrames = static_cast<int>(converted.size() / static_cast<size_t>(channels));
        window.samples         = std::move(converted);
        window.frameCount      = outputFrames;
        if(outputFormat.isValid()) {
            outputFormat.setSampleFormat(SampleFormat::F32);
            window.format = outputFormat;
        }

        m_expectedStartMs   = inputEndMs;
        m_haveExpectedStart = true;
        return outputFrames > 0;
    }

private:
    ResamplerDsp m_resampler;
    AudioFormat m_inputFormat;
    int m_targetRate{0};
    uint64_t m_expectedStartMs{0};
    bool m_haveExpectedStart{false};
};

class VisualisationSessionPrivate
{
public:
    VisualisationSessionPrivate(std::shared_ptr<VisualisationBackend> backend_, VisualisationService* service_)
        : backend{std::move(backend_)}
        , service{service_}
        , token{this->backend ? this->backend->registerSession() : 0}
    {
        if(this->service) {
            Q_EMIT this->service->sessionActivityChanged();
        }
    }

    ~VisualisationSessionPrivate()
    {
        if(backend && token != 0) {
            backend->unregisterSession(token);
        }
        if(service) {
            Q_EMIT service->sessionActivityChanged();
        }
    }

    std::shared_ptr<VisualisationBackend> backend;
    QPointer<VisualisationService> service;
    VisualisationBackend::SessionToken token{0};
    VisualisationSession::ChannelSelection channelSelection;
    PcmWindowResampler pcmResampler;
};

VisualisationSession::VisualisationSession(std::shared_ptr<VisualisationBackend> backend, VisualisationService* service)
    : p{std::make_unique<VisualisationSessionPrivate>(std::move(backend), service)}
{ }

VisualisationSession::~VisualisationSession() = default;

void VisualisationSession::requestBacklog(uint64_t durationMs)
{
    if(p->backend && p->token != 0) {
        p->backend->requestBacklog(p->token, durationMs);
    }
}

void VisualisationSession::setChannelSelection(ChannelSelection selection)
{
    p->channelSelection = selection;
}

void VisualisationSession::setPcmTargetSampleRate(const int sampleRate)
{
    p->pcmResampler.setTargetRate(sampleRate);
}

uint64_t VisualisationSession::currentTimeMs() const
{
    return p->backend ? p->backend->currentTimeMs() : 0;
}

bool VisualisationSession::getPcmWindow(PcmWindow& out, uint64_t centerTimeMs, uint64_t durationMs) const
{
    return p->backend && p->backend->getPcmWindow(out, centerTimeMs, durationMs, p->channelSelection)
        && p->pcmResampler.process(out);
}

bool VisualisationSession::getPcmWindowEndingAt(PcmWindow& out, uint64_t endTimeMs, uint64_t durationMs) const
{
    return p->backend && p->backend->getPcmWindowEndingAt(out, endTimeMs, durationMs, p->channelSelection)
        && p->pcmResampler.process(out);
}

bool VisualisationSession::getSpectrumWindow(SpectrumWindow& out, uint64_t centerTimeMs, int fftSize,
                                             SpectrumWindowFunction windowFunction) const
{
    return p->backend && p->backend->getSpectrumWindow(out, centerTimeMs, fftSize, p->channelSelection, windowFunction);
}

bool VisualisationSession::getSpectrumWindowEndingAt(SpectrumWindow& out, uint64_t endTimeMs, int fftSize,
                                                     SpectrumWindowFunction windowFunction) const
{
    return p->backend
        && p->backend->getSpectrumWindowEndingAt(out, endTimeMs, fftSize, p->channelSelection, windowFunction);
}

class VisualisationServicePrivate
{
public:
    std::shared_ptr<VisualisationBackend> backend{std::make_shared<VisualisationBackend>()};
};

VisualisationService::VisualisationService(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<VisualisationServicePrivate>()}
{ }

VisualisationService::~VisualisationService() = default;

VisualisationSessionPtr VisualisationService::createSession()
{
    return VisualisationSessionPtr{new VisualisationSession{p->backend, this}};
}

std::shared_ptr<VisualisationBackend> VisualisationService::backend() const
{
    return p->backend;
}

bool VisualisationService::hasActiveSessions() const
{
    return p->backend && p->backend->hasActiveSessions();
}
} // namespace Fooyin
