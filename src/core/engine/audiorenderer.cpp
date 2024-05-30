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

#include "audiorenderer.h"

#include <core/engine/audiobuffer.h>
#include <core/engine/audiooutput.h>
#include <utils/threadqueue.h>

#include <QDebug>
#include <QTimer>
#include <utility>

namespace Fooyin {
struct AudioRenderer::Private
{
    AudioRenderer* self;

    std::unique_ptr<AudioOutput> audioOutput;
    AudioFormat format;
    double volume{0.0};
    int bufferSize{0};

    bool bufferPrefilled{false};

    ThreadQueue<AudioBuffer> bufferQueue{false};
    AudioBuffer tempBuffer;
    int totalSamplesWritten{0};
    int currentBufferOffset{0};

    bool isRunning{false};

    QTimer* writeTimer;

    explicit Private(AudioRenderer* self_)
        : self{self_}
        , writeTimer{new QTimer(self)}
    {
        QObject::connect(writeTimer, &QTimer::timeout, self, [this]() { writeNext(); });
    }

    bool canWrite()
    {
        return isRunning && audioOutput->initialised();
    }

    bool initOutput()
    {
        if(!audioOutput->init(format)) {
            return false;
        }

        audioOutput->setVolume(volume);
        bufferSize = audioOutput->bufferSize();
        updateInterval();

        return true;
    }

    void resetBuffer()
    {
        bufferPrefilled     = false;
        totalSamplesWritten = 0;
        currentBufferOffset = 0;
        bufferQueue.clear();
        tempBuffer.reset();
    }

    void outputStateChanged(AudioOutput::State state)
    {
        if(state == AudioOutput::State::Disconnected) {
            emit self->outputStateChanged(state);
            audioOutput->uninit();
            bufferPrefilled = false;
        }
    }

    void updateOutputVolume(double newVolume)
    {
        volume = newVolume;

        if(audioOutput) {
            audioOutput->setVolume(volume);
        }
    }

    void updateInterval() const
    {
        const auto interval = static_cast<int>(static_cast<double>(bufferSize) / format.sampleRate() * 1000 * 0.25);
        writeTimer->setInterval(interval);
    }

    void writeNext()
    {
        if(!canWrite() || bufferQueue.empty()) {
            return;
        }

        const int samples = audioOutput->currentState().freeSamples;

        if((samples == 0 && totalSamplesWritten > 0) || (samples > 0 && renderAudio(samples) == samples)) {
            if(canWrite() && !bufferPrefilled) {
                bufferPrefilled = true;
                audioOutput->start();
            }
        }
    }

    int writeAudioSamples(int samples)
    {
        tempBuffer.clear();

        int samplesBuffered{0};

        const int sstride = format.bytesPerFrame();

        while(isRunning && !bufferQueue.empty() && samplesBuffered < samples) {
            const AudioBuffer& buffer = bufferQueue.front();

            if(!buffer.isValid()) {
                // End of file
                currentBufferOffset = 0;
                bufferQueue.dequeue();
                QMetaObject::invokeMethod(self, &AudioRenderer::finished);
                return samplesBuffered;
            }

            const int bytesLeft = buffer.byteCount() - currentBufferOffset;

            if(bytesLeft <= 0) {
                currentBufferOffset = 0;
                emit self->bufferProcessed(buffer);
                bufferQueue.dequeue();
                continue;
            }

            const int sampleCount = std::min(bytesLeft / sstride, samples - samplesBuffered);
            const int bytes       = sampleCount * sstride;
            const auto fdata      = buffer.constData().subspan(currentBufferOffset, static_cast<size_t>(bytes));

            if(!tempBuffer.isValid()) {
                tempBuffer = {fdata, buffer.format(), buffer.startTime()};
            }
            else {
                tempBuffer.append(fdata);
            }

            samplesBuffered += sampleCount;
            currentBufferOffset += bytes;
        }

        tempBuffer.fillRemainingWithSilence();

        if(!tempBuffer.isValid()) {
            return 0;
        }

        return samplesBuffered;
    }

    int renderAudio(int samples)
    {
        if(writeAudioSamples(samples) == 0) {
            return 0;
        }

        if(!tempBuffer.isValid()) {
            return 0;
        }

        const int samplesWritten = audioOutput->write(tempBuffer);
        totalSamplesWritten += samplesWritten;

        return samplesWritten;
    }
};

AudioRenderer::AudioRenderer(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this)}
{
    setObjectName(QStringLiteral("Renderer"));
}

AudioRenderer::~AudioRenderer()
{
    if(p->audioOutput && p->audioOutput->initialised()) {
        p->audioOutput->uninit();
    }
}

bool AudioRenderer::init(const AudioFormat& format)
{
    p->format = format;

    if(!p->audioOutput) {
        return false;
    }

    if(p->audioOutput->initialised()) {
        p->audioOutput->uninit();
    }

    return p->initOutput();
}

void AudioRenderer::start()
{
    if(std::exchange(p->isRunning, true)) {
        return;
    }

    p->writeTimer->start();
}

void AudioRenderer::stop()
{
    p->isRunning = false;
    p->writeTimer->stop();

    p->resetBuffer();
}

void AudioRenderer::closeOutput()
{
    if(p->audioOutput->initialised()) {
        p->audioOutput->uninit();
    }
}

void AudioRenderer::reset()
{
    if(p->audioOutput && p->audioOutput->initialised()) {
        p->audioOutput->reset();
    }

    p->resetBuffer();
}

bool AudioRenderer::isPaused() const
{
    return !p->isRunning;
}

void AudioRenderer::pause(bool paused)
{
    if(p->audioOutput && p->audioOutput->initialised()) {
        p->audioOutput->setPaused(paused);
    }

    p->isRunning = !paused;

    if(paused) {
        p->writeTimer->stop();
    }
    else {
        p->writeTimer->start();
    }
}

void AudioRenderer::queueBuffer(const AudioBuffer& buffer)
{
    p->bufferQueue.enqueue(buffer);
}

void AudioRenderer::updateOutput(const OutputCreator& output)
{
    auto newOutput = output();
    if(newOutput == p->audioOutput) {
        return;
    }

    if(p->audioOutput && p->audioOutput->initialised()) {
        p->audioOutput->uninit();
    }

    p->audioOutput     = std::move(newOutput);
    p->bufferPrefilled = false;
    QObject::connect(p->audioOutput.get(), &AudioOutput::stateChanged, this,
                     [this](const auto state) { p->outputStateChanged(state); });
}

void AudioRenderer::updateDevice(const QString& device)
{
    if(!p->audioOutput) {
        return;
    }

    p->bufferPrefilled = false;

    if(p->audioOutput->initialised()) {
        p->audioOutput->uninit();
        p->audioOutput->setDevice(device);
    }
    else {
        p->audioOutput->setDevice(device);
    }
}

void AudioRenderer::updateVolume(double volume)
{
    p->initialVolume = volume;
    p->updateOutputVolume(volume);
}

    if(p->audioOutput && p->audioOutput->canHandleVolume()) {
        p->audioOutput->setVolume(volume);
    }
}
} // namespace Fooyin

#include "moc_audiorenderer.cpp"
