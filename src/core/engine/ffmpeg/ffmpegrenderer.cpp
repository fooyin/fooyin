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

#include "ffmpegcodec.h"
#include "ffmpegframe.h"
#include "ffmpegutils.h"

#include <core/engine/audiooutput.h>
#include <utils/threadqueue.h>

#include <QDebug>
#include <QTimer>
#include <utility>

namespace Fy::Core::Engine::FFmpeg {
struct Renderer::Private
{
    Renderer* renderer;

    Codec* codec{nullptr};

    AudioOutput* audioOutput{nullptr};
    OutputContext outputContext;

    bool bufferPrefilled{false};

    Utils::ThreadQueue<Frame> frameQueue{false};
    std::vector<uint8_t> tempBuffer;

    double volume{1.0};

    Private(Renderer* renderer)
        : renderer{renderer}
    {
        outputContext.writeAudioToBuffer = [this](uint8_t* data, int samples) {
            return writeAudioToBuffer(data, samples);
        };
    }

    bool updateOutput()
    {
        if(!audioOutput) {
            return false;
        }

        if(!codec || !codec->context()) {
            return false;
        }

        outputContext.format        = interleaveFormat(codec->context()->sample_fmt);
        outputContext.sampleRate    = codec->context()->sample_rate;
        outputContext.channelLayout = codec->context()->ch_layout;
        outputContext.sstride = av_get_bytes_per_sample(outputContext.format) * outputContext.channelLayout.nb_channels;
        outputContext.volume  = volume;

        return audioOutput->init(outputContext);
    }

    int writeAudioSamples(int samples)
    {
        int samplesBuffered = 0;

        const int sstride = outputContext.sstride;
        tempBuffer.reserve(samples * sstride);

        while(!renderer->isPaused() && samplesBuffered < samples) {
            const Frame& frame = frameQueue.front();

            if(!frame.isValid()) {
                renderer->setAtEnd(true);
                return samplesBuffered;
            }

            if(frame.avFrame()->nb_samples <= 0) {
                emit renderer->frameProcessed(frameQueue.dequeue());
                continue;
            }

            uint8_t** fdata       = frame.avFrame()->data;
            const int sampleCount = std::min(frame.sampleCount(), samples - samplesBuffered);

            std::copy_n(fdata[0], sampleCount * sstride, std::back_inserter(tempBuffer));

            samplesBuffered += sampleCount;
            skipSamples(frame.avFrame(), sampleCount);
        }

        fillSilence(tempBuffer.data() + samplesBuffered * sstride, (samples - samplesBuffered) * sstride,
                    outputContext.format);

        return samplesBuffered;
    }

    int renderAudio(int samples)
    {
        const int samplesWritten = writeAudioSamples(samples);

        if(!audioOutput->canHandleVolume()) {
            adjustVolumeOfSamples(tempBuffer.data(), outputContext.format, samples * outputContext.sstride, volume);
        }

        audioOutput->write(tempBuffer.data(), samplesWritten);

        tempBuffer.clear();
        return samplesWritten;
    }

    int writeAudioToBuffer(uint8_t* data, int samples)
    {
        const int samplesWritten = writeAudioSamples(samples);

        if(!audioOutput->canHandleVolume()) {
            adjustVolumeOfSamples(tempBuffer.data(), outputContext.format, samples * outputContext.sstride, volume);
        }

        std::copy_n(tempBuffer.data(), samples * outputContext.sstride, data);

        tempBuffer.clear();
        return samplesWritten;
    }
};

Renderer::Renderer(QObject* parent)
    : EngineWorker{parent}
    , p{std::make_unique<Private>(this)}
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
    p->frameQueue.clear();
    p->tempBuffer.clear();
}

void Renderer::kill()
{
    EngineWorker::kill();

    if(p->audioOutput && p->audioOutput->initialised()) {
        p->audioOutput->uninit();
    }

    p->bufferPrefilled = false;
    p->frameQueue.clear();
    p->tempBuffer.clear();
}

void Renderer::render(Frame frame)
{
    p->frameQueue.enque(std::move(frame));

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
    if(AudioOutput* prevOutput = std::exchange(p->audioOutput, output)) {
        if(prevOutput->initialised()) {
            prevOutput->uninit();
        }
    }
    p->bufferPrefilled = false;
    p->updateOutput();
}

void Renderer::updateDevice(const QString& device)
{
    if(p->audioOutput) {
        if(p->audioOutput->initialised()) {
            p->audioOutput->setDevice(device);
            p->audioOutput->uninit();
            p->audioOutput->init(p->outputContext);
            p->bufferPrefilled = false;
        }
        else {
            p->audioOutput->setDevice(device);
        }
    }
}

void Renderer::updateVolume(double volume)
{
    p->volume = volume;
    if(p->audioOutput) {
        p->audioOutput->setVolume(volume);
    }
}

bool Renderer::canDoNextStep() const
{
    return !p->frameQueue.empty() && EngineWorker::canDoNextStep();
}

int Renderer::timerInterval() const
{
    return ((p->audioOutput->bufferSize() / static_cast<double>(p->outputContext.sampleRate)) * 0.25) * 1000;
}

void Renderer::doNextStep()
{
    if(isPaused()) {
        return;
    }

    if(p->audioOutput->type() == OutputType::Push) {
        const int samples = p->audioOutput->currentState().freeSamples;
        if(samples > 0 && p->renderAudio(samples) == samples) {
            if(!p->bufferPrefilled) {
                p->bufferPrefilled = true;
                p->audioOutput->start();
            }
        }
    }
    else if(!p->bufferPrefilled) {
        p->bufferPrefilled = true;
        p->audioOutput->start();
    }
    scheduleNextStep(false);
}
} // namespace Fy::Core::Engine::FFmpeg
