/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "ffmpegrenderer.h"

#include "core/engine/audiooutput.h"
#include "ffmpegclock.h"
#include "ffmpegcodec.h"
#include "ffmpegframe.h"
#include "ffmpegutils.h"

#include <QDebug>
#include <QTimer>

#include <queue>

namespace Fy::Core::Engine::FFmpeg {
struct Renderer::Private
{
    Renderer* renderer;

    Codec* codec;
    AudioClock* clock;

    AudioOutput* audioOutput;
    OutputContext outputContext;

    bool bufferPrefilled{false};

    std::queue<Frame> frameQueue;
    std::vector<uint8_t> tempBuffer;

    Private(Renderer* renderer, AudioClock* clock)
        : renderer{renderer}
        , codec{nullptr}
        , clock{clock}
        , audioOutput{nullptr}
    { }

    bool updateOutput()
    {
        if(!audioOutput || !codec->context()) {
            return false;
        }
        outputContext.format        = interleaveFormat(codec->context()->sample_fmt);
        outputContext.sampleRate    = codec->context()->sample_rate;
        outputContext.channelLayout = codec->context()->ch_layout;
        outputContext.sstride = av_get_bytes_per_sample(outputContext.format) * outputContext.channelLayout.nb_channels;

        return audioOutput->init(&outputContext);
    }

    int renderAudio(int samples)
    {
        if(renderer->isPaused()) {
            return 0;
        }

        int samplesBuffered{0};

        while(samplesBuffered < samples) {
            if(renderer->isPaused()) {
                return samplesBuffered;
            }

            const Frame& frame = frameQueue.front();

            if(!frame.isValid()) {
                renderer->setAtEnd(true);
                return samplesBuffered;
            }

            if(frame.avFrame()->nb_samples <= 0) {
                const uint64_t pos = frame.ptsMs();
                if(pos > 0) {
                    clock->sync(frame.ptsMs());
                }
                frameQueue.pop();
                emit renderer->frameProcessed();
                continue;
            }

            uint8_t** fdata       = frame.avFrame()->data;
            const int sstride     = outputContext.sstride;
            const int sampleCount = std::min(frame.sampleCount(), samples - samplesBuffered);

            tempBuffer.reserve(sampleCount * sstride);
            std::copy(fdata[0], fdata[0] + sampleCount * sstride, std::back_inserter(tempBuffer));

            samplesBuffered += sampleCount;
            skipSamples(frame.avFrame(), sampleCount);
        }
        audioOutput->write(&outputContext, tempBuffer.data(), samples);
        tempBuffer.clear();

        return samples;
    }
};

Renderer::Renderer(AudioClock* clock, QObject* parent)
    : EngineWorker{parent}
    , p{std::make_unique<Private>(this, clock)}
{
    setObjectName("Renderer");
}

Renderer::~Renderer() = default;

void Renderer::run(Codec* codec, AudioOutput* output)
{
    p->codec       = codec;
    p->audioOutput = output;
    setPaused(false);
    p->updateOutput();
    scheduleNextStep();
}

void Renderer::reset()
{
    EngineWorker::reset();

    if(p->audioOutput) {
        p->audioOutput->reset();
    }

    p->bufferPrefilled = false;
    p->frameQueue      = {};
    p->tempBuffer.clear();
}

void Renderer::kill()
{
    EngineWorker::kill();

    if(p->audioOutput) {
        p->audioOutput->uninit();
    }

    p->bufferPrefilled = false;
    p->frameQueue      = {};
    p->tempBuffer.clear();
}

void Renderer::render(Frame frame)
{
    p->frameQueue.emplace(frame);

    if(p->frameQueue.size() == 1) {
        scheduleNextStep();
    }
}

void Renderer::pauseOutput(bool isPaused)
{
    p->audioOutput->setPaused(isPaused);
}

void Renderer::updateOutput(AudioOutput* output)
{
    if(isPaused()) {
        if(AudioOutput* prevOutput = std::exchange(p->audioOutput, output)) {
            prevOutput->uninit();
            p->bufferPrefilled = false;
            p->updateOutput();
        }
    }
}

void Renderer::updateDevice(const QString& /*device*/) { }

bool Renderer::canDoNextStep() const
{
    return !p->frameQueue.empty() && EngineWorker::canDoNextStep();
}

int Renderer::timerInterval() const
{
    return ((p->outputContext.bufferSize / static_cast<double>(p->outputContext.sampleRate)) * 0.25) * 1000;
}

void Renderer::doNextStep()
{
    if(isPaused()) {
        return;
    }
    const int samples = p->audioOutput->currentState(&p->outputContext).freeSamples;
    if(samples > 0) {
        if(p->renderAudio(samples) == samples) {
            if(!p->bufferPrefilled) {
                p->bufferPrefilled = true;
                p->audioOutput->start();
            }
        }
    }
    scheduleNextStep(false);
}
} // namespace Fy::Core::Engine::FFmpeg
