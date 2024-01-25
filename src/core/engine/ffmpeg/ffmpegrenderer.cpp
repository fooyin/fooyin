/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "ffmpegaudiobuffer.h"
#include "ffmpegutils.h"

#include <core/engine/audiooutput.h>
#include <utils/threadqueue.h>

#include <QDebug>
#include <QTimer>
#include <utility>

namespace Fooyin {
struct Renderer::Private
{
    Renderer* self;

    AudioOutput* audioOutput{nullptr};
    OutputContext outputContext;

    bool bufferPrefilled{false};

    ThreadQueue<FFmpegAudioBuffer> bufferQueue{false};
    std::vector<uint8_t> tempBuffer;
    int totalSamplesWritten{0};
    int currentBufferOffset{0};

    explicit Private(Renderer* renderer)
        : self{renderer}
    {
        outputContext.writeAudioToBuffer = [this](uint8_t* data, int samples) {
            return writeAudioToBuffer(data, samples);
        };
    }

    void updateContext(const OutputContext& context)
    {
        outputContext.channelLayout = context.channelLayout;
        outputContext.format        = context.format;
        outputContext.volume        = context.volume;
    }

    int writeAudioSamples(int samples)
    {
        int samplesBuffered = 0;

        const int sstride = outputContext.format.bytesPerFrame();
        tempBuffer.reserve(static_cast<int>(samples * sstride));

        while(!self->isPaused() && !bufferQueue.empty() && samplesBuffered < samples) {
            const FFmpegAudioBuffer& buffer = bufferQueue.front();

            if(!buffer.isValid()) {
                currentBufferOffset = 0;
                self->setAtEnd(true);
                return samplesBuffered;
            }

            const int bytesLeft = buffer.byteCount() - currentBufferOffset;

            if(bytesLeft <= 0) {
                currentBufferOffset = 0;
                QMetaObject::invokeMethod(self, "audioBufferProcessed", Q_ARG(const FFmpegAudioBuffer&, buffer));
                bufferQueue.dequeue();
                continue;
            }

            const uint8_t* fdata  = buffer.constData() + currentBufferOffset;
            const int sampleCount = std::min(bytesLeft / sstride, samples - samplesBuffered);

            std::copy_n(fdata, sampleCount * sstride, std::back_inserter(tempBuffer));

            samplesBuffered += sampleCount;
            currentBufferOffset += sampleCount * sstride;
        }

        Utils::fillSilence(tempBuffer.data() + static_cast<int>(samplesBuffered * sstride),
                           (samples - samplesBuffered) * sstride, outputContext.format);

        return samplesBuffered;
    }

    int renderAudio(int samples)
    {
        int samplesWritten = writeAudioSamples(samples);

        if(!audioOutput->canHandleVolume()) {
            Utils::adjustVolumeOfSamples(tempBuffer.data(), outputContext.format,
                                         samples * outputContext.format.bytesPerFrame(), outputContext.volume);
        }

        samplesWritten = audioOutput->write(tempBuffer.data(), samplesWritten);
        totalSamplesWritten += samplesWritten;

        tempBuffer.clear();
        return samplesWritten;
    }

    int writeAudioToBuffer(uint8_t* data, int samples)
    {
        if(self->isPaused()) {
            return 0;
        }

        const int samplesWritten = writeAudioSamples(samples);

        const int sstride = outputContext.format.bytesPerFrame();

        if(!audioOutput->canHandleVolume()) {
            Utils::adjustVolumeOfSamples(tempBuffer.data(), outputContext.format, samples * sstride,
                                         outputContext.volume);
        }

        std::copy_n(tempBuffer.data(), samples * sstride, data);

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

void Renderer::run(const OutputContext& context, AudioOutput* output)
{
    p->updateContext(context);
    p->audioOutput = output;
    setPaused(false);
    p->audioOutput->init(p->outputContext);
    scheduleNextStep();
}

void Renderer::reset()
{
    EngineWorker::reset();

    if(p->audioOutput) {
        p->audioOutput->reset();
    }

    p->bufferPrefilled     = false;
    p->totalSamplesWritten = 0;
    p->bufferQueue.clear();
    p->tempBuffer.clear();
}

void Renderer::kill()
{
    EngineWorker::kill();

    p->bufferPrefilled     = false;
    p->totalSamplesWritten = 0;
    p->bufferQueue.clear();
    p->tempBuffer.clear();
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

    if(!isPaused()) {
        p->audioOutput->init(p->outputContext);
    }
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
    p->outputContext.volume = volume;
    if(p->audioOutput) {
        p->audioOutput->setVolume(volume);
    }
}

void Renderer::render(const FFmpegAudioBuffer& frame)
{
    p->bufferQueue.enqueue(frame);

    if(p->bufferQueue.size() == 1) {
        scheduleNextStep();
    }
}

bool Renderer::canDoNextStep() const
{
    return !p->bufferQueue.empty() && EngineWorker::canDoNextStep();
}

int Renderer::timerInterval() const
{
    return static_cast<int>(
        ((p->audioOutput->bufferSize() / static_cast<double>(p->outputContext.format.sampleRate())) * 0.25) * 1000);
}

void Renderer::doNextStep()
{
    if(isPaused()) {
        return;
    }

    if(p->audioOutput->type() == OutputType::Push) {
        const int samples = p->audioOutput->currentState().freeSamples;
        if((samples == 0 && p->totalSamplesWritten > 0) || (samples > 0 && p->renderAudio(samples) == samples)) {
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
} // namespace Fooyin

#include "moc_ffmpegrenderer.cpp"
