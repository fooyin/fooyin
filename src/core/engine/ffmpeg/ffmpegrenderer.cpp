/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audiobuffer.h>
#include <core/engine/audiooutput.h>
#include <utils/threadqueue.h>

#include <QDebug>
#include <QTimer>
#include <utility>

namespace Fooyin {
struct FFmpegRenderer::Private
{
    FFmpegRenderer* self;

    AudioOutput* audioOutput{nullptr};
    OutputContext outputContext;

    bool bufferPrefilled{false};

    std::mutex bufferMutex;
    ThreadQueue<AudioBuffer> bufferQueue{false};
    AudioBuffer tempBuffer;
    int totalSamplesWritten{0};
    int currentBufferOffset{0};

    bool isRunning{false};

    QTimer timer;

    explicit Private(FFmpegRenderer* renderer)
        : self{renderer}
    {
        outputContext.writeAudioToBuffer = [this](uint8_t* data, int samples) {
            return writeAudioToBuffer(data, samples);
        };

        QObject::connect(&timer, &QTimer::timeout, self, [this]() { writeNext(); });
    }

    void updateInterval()
    {
        const auto interval = static_cast<int>(
            ((audioOutput->bufferSize() / static_cast<double>(outputContext.format.sampleRate())) * 0.25) * 1000);
        timer.setInterval(interval);
    }

    void updateContext(const OutputContext& context)
    {
        outputContext.format = context.format;
        outputContext.volume = context.volume;
    }

    void writeNext()
    {
        if(!isRunning || bufferQueue.empty()) {
            return;
        }

        if(audioOutput->type() == OutputType::Push) {
            const int samples = audioOutput->currentState().freeSamples;
            if((samples == 0 && totalSamplesWritten > 0) || (samples > 0 && renderAudio(samples) == samples)) {
                if(!bufferPrefilled) {
                    bufferPrefilled = true;
                    audioOutput->start();
                }
            }
        }
        else if(!bufferPrefilled) {
            bufferPrefilled = true;
            audioOutput->start();
        }
    }

    int writeAudioSamples(int samples)
    {
        tempBuffer.clear();

        int samplesBuffered{0};

        const int sstride = outputContext.format.bytesPerFrame();

        while(isRunning && !bufferQueue.empty() && samplesBuffered < samples) {
            const AudioBuffer& buffer = bufferQueue.front();

            if(!buffer.isValid()) {
                // End of file
                currentBufferOffset = 0;
                bufferQueue.dequeue();
                QMetaObject::invokeMethod(self, &FFmpegRenderer::finished);
                return samplesBuffered;
            }

            const int bytesLeft = buffer.byteCount() - currentBufferOffset;

            if(bytesLeft <= 0) {
                currentBufferOffset = 0;
                QMetaObject::invokeMethod(self, "bufferProcessed", Q_ARG(const AudioBuffer&, buffer));
                bufferQueue.dequeue();
                continue;
            }

            const int sampleCount = std::min(bytesLeft / sstride, samples - samplesBuffered);
            const int bytes       = sampleCount * sstride;
            const auto fdata      = buffer.constData().subspan(currentBufferOffset, static_cast<size_t>(bytes));

            if(!tempBuffer.isValid()) {
                tempBuffer = {fdata, buffer.format(), buffer.startTime()};
                tempBuffer.reserve(static_cast<int>(samples * sstride));
            }
            else {
                tempBuffer.append(fdata);
            }

            samplesBuffered += sampleCount;
            currentBufferOffset += sampleCount * sstride;
        }

        tempBuffer.fillRemainingWithSilence();

        if(!tempBuffer.isValid()) {
            return 0;
        }

        return samplesBuffered;
    }

    int renderAudio(int samples)
    {
        const std::lock_guard<std::mutex> lock(bufferMutex);

        if(writeAudioSamples(samples) == 0) {
            return 0;
        }

        if(!audioOutput->canHandleVolume()) {
            tempBuffer.adjustVolumeOfSamples(outputContext.volume);
        }

        if(!tempBuffer.isValid()) {
            return 0;
        }

        const int samplesWritten = audioOutput->write(tempBuffer);
        totalSamplesWritten += samplesWritten;

        return samplesWritten;
    }

    int writeAudioToBuffer(uint8_t* data, int samples)
    {
        if(!isRunning) {
            return 0;
        }

        const std::lock_guard<std::mutex> lock(bufferMutex);

        const int samplesWritten = writeAudioSamples(samples);
        const int sstride        = tempBuffer.format().bytesPerFrame();

        if(!audioOutput->canHandleVolume()) {
            tempBuffer.adjustVolumeOfSamples(outputContext.volume);
        }

        if(!tempBuffer.isValid()) {
            return 0;
        }

        std::copy_n(std::bit_cast<uint8_t*>(tempBuffer.constData().data()), samples * sstride, data);

        return samplesWritten;
    }
};

FFmpegRenderer::FFmpegRenderer(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this)}
{
    setObjectName("Renderer");
}

FFmpegRenderer::~FFmpegRenderer() = default;

void FFmpegRenderer::init(const OutputContext& context)
{
    if(p->outputContext != context) {
        p->updateContext(context);
    }

    if(p->audioOutput) {
        if(p->audioOutput->initialised()) {
            p->audioOutput->uninit();
        }
        p->audioOutput->init(p->outputContext);
    }
}

void FFmpegRenderer::start()
{
    if(std::exchange(p->isRunning, true)) {
        return;
    }

    p->updateInterval();
    p->timer.start();
}

void FFmpegRenderer::stop()
{
    p->isRunning = false;

    p->timer.stop();
    p->bufferPrefilled     = false;
    p->totalSamplesWritten = 0;
    p->bufferQueue.clear();
    p->tempBuffer.reset();
}

void FFmpegRenderer::pause(bool paused)
{
    p->isRunning = paused;
}

int FFmpegRenderer::queuedBuffers() const
{
    return static_cast<int>(p->bufferQueue.size());
}

void FFmpegRenderer::queueBuffer(const AudioBuffer& buffer)
{
    p->bufferQueue.enqueue(buffer);
}

void FFmpegRenderer::pauseOutput(bool isPaused)
{
    p->audioOutput->setPaused(isPaused);
}

void FFmpegRenderer::updateOutput(AudioOutput* output)
{
    if(AudioOutput* prevOutput = std::exchange(p->audioOutput, output)) {
        if(prevOutput->initialised()) {
            prevOutput->uninit();
        }
    }

    p->bufferPrefilled = false;

    if(p->isRunning) {
        p->audioOutput->init(p->outputContext);
    }
}

void FFmpegRenderer::updateDevice(const QString& device)
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

void FFmpegRenderer::updateVolume(double volume)
{
    p->outputContext.volume = volume;
    if(p->audioOutput) {
        p->audioOutput->setVolume(volume);
    }
}
} // namespace Fooyin

#include "moc_ffmpegrenderer.cpp"
