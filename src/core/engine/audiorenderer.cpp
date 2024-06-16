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

#include <QBasicTimer>
#include <QDebug>
#include <QTimer>
#include <QTimerEvent>

#include <utility>

using namespace std::chrono_literals;

constexpr auto FadeInterval = 10;

namespace Fooyin {
struct AudioRenderer::Private
{
    AudioRenderer* m_self;

    std::unique_ptr<AudioOutput> m_audioOutput;
    AudioFormat m_format;
    double m_volume{0.0};
    int m_bufferSize{0};

    bool m_bufferPrefilled{false};

    ThreadQueue<AudioBuffer> m_bufferQueue{false};
    AudioBuffer m_tempBuffer;
    int m_totalSamplesWritten{0};
    int m_currentBufferOffset{0};

    bool m_isRunning{false};

    QTimer* m_writeTimer;

    QBasicTimer m_fadeTimer;
    int m_fadeLength{0};
    int m_fadeSteps{0};
    int m_currentFadeStep{0};
    double m_volumeChange{0.0};
    double m_initialVolume{0.0};

    explicit Private(AudioRenderer* self)
        : m_self{self}
        , m_writeTimer{new QTimer(m_self)}
    {
        QObject::connect(m_writeTimer, &QTimer::timeout, m_self, [this]() { writeNext(); });
    }

    void resetFade(int length)
    {
        if(m_fadeTimer.isActive()) {
            m_fadeTimer.stop();
        }

        m_volumeChange    = 0;
        m_currentFadeStep = 0;
        m_fadeLength      = length;
        m_fadeSteps       = static_cast<int>(static_cast<double>(m_fadeLength) / FadeInterval);
    }

    bool canWrite() const
    {
        return m_isRunning && m_audioOutput->initialised();
    }

    bool initOutput()
    {
        if(!m_audioOutput->init(m_format)) {
            return false;
        }

        m_audioOutput->setVolume(m_volume);
        m_bufferSize = m_audioOutput->bufferSize();
        updateInterval();

        return true;
    }

    void resetBuffer()
    {
        m_bufferPrefilled     = false;
        m_totalSamplesWritten = 0;
        m_currentBufferOffset = 0;
        m_bufferQueue.clear();
        m_tempBuffer.reset();
    }

    void outputStateChanged(AudioOutput::State state)
    {
        if(state == AudioOutput::State::Disconnected) {
            emit m_self->outputStateChanged(state);
            m_audioOutput->uninit();
            m_bufferPrefilled = false;
        }
    }

    void pauseOutput(bool pause) const
    {
        if(m_audioOutput && m_audioOutput->initialised()) {
            m_audioOutput->setPaused(pause);
        }
    }

    void pause()
    {
        m_isRunning = false;
        m_writeTimer->stop();

        updateOutputVolume(0.0);

        m_audioOutput->drain();
        emit m_self->paused();
        pauseOutput(true);
    }

    void updateOutputVolume(double newVolume)
    {
        m_volume = newVolume;

        if(m_audioOutput && m_audioOutput->initialised()) {
            m_audioOutput->setVolume(m_volume);
        }
    }

    void updateInterval() const
    {
        const auto interval = static_cast<int>(static_cast<double>(m_bufferSize) / m_format.sampleRate() * 1000 * 0.25);
        m_writeTimer->setInterval(interval);
    }

    void writeNext()
    {
        if(!canWrite() || m_bufferQueue.empty()) {
            return;
        }

        const int samples = m_audioOutput->currentState().freeSamples;

        if((samples == 0 && m_totalSamplesWritten > 0) || (samples > 0 && renderAudio(samples) == samples)) {
            if(canWrite() && !m_bufferPrefilled) {
                m_bufferPrefilled = true;
                m_audioOutput->start();
            }
        }
    }

    int writeAudioSamples(int samples)
    {
        m_tempBuffer.clear();

        int samplesBuffered{0};

        const int sstride = m_format.bytesPerFrame();

        while(m_isRunning && !m_bufferQueue.empty() && samplesBuffered < samples) {
            const AudioBuffer& buffer = m_bufferQueue.front();

            if(!buffer.isValid()) {
                // End of file
                m_currentBufferOffset = 0;
                m_bufferQueue.dequeue();
                QMetaObject::invokeMethod(m_self, &AudioRenderer::finished);
                return samplesBuffered;
            }

            const int bytesLeft = buffer.byteCount() - m_currentBufferOffset;

            if(bytesLeft <= 0) {
                m_currentBufferOffset = 0;
                emit m_self->bufferProcessed(buffer);
                m_bufferQueue.dequeue();
                continue;
            }

            const int sampleCount = std::min(bytesLeft / sstride, samples - samplesBuffered);
            const int bytes       = sampleCount * sstride;
            const auto fdata      = buffer.constData().subspan(m_currentBufferOffset, static_cast<size_t>(bytes));

            if(!m_tempBuffer.isValid()) {
                m_tempBuffer = {fdata, buffer.format(), buffer.startTime()};
            }
            else {
                m_tempBuffer.append(fdata);
            }

            samplesBuffered += sampleCount;
            m_currentBufferOffset += bytes;
        }

        m_tempBuffer.fillRemainingWithSilence();

        if(!m_tempBuffer.isValid()) {
            return 0;
        }

        return samplesBuffered;
    }

    int renderAudio(int samples)
    {
        if(writeAudioSamples(samples) == 0) {
            return 0;
        }

        if(!m_tempBuffer.isValid()) {
            return 0;
        }

        const int samplesWritten = m_audioOutput->write(m_tempBuffer);
        m_totalSamplesWritten += samplesWritten;

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
    if(p->m_audioOutput && p->m_audioOutput->initialised()) {
        p->m_audioOutput->uninit();
    }
}

bool AudioRenderer::init(const AudioFormat& format)
{
    p->m_format = format;

    if(!p->m_audioOutput) {
        return false;
    }

    if(p->m_audioOutput->initialised()) {
        p->m_audioOutput->uninit();
    }

    return p->initOutput();
}

void AudioRenderer::start()
{
    if(std::exchange(p->m_isRunning, true)) {
        return;
    }

    p->m_writeTimer->start();
}

void AudioRenderer::stop()
{
    p->m_isRunning = false;
    p->m_writeTimer->stop();

    p->resetFade(0);
    p->resetBuffer();
}

void AudioRenderer::closeOutput()
{
    if(p->m_audioOutput->initialised()) {
        p->m_audioOutput->uninit();
    }
}

void AudioRenderer::reset()
{
    if(p->m_audioOutput && p->m_audioOutput->initialised()) {
        p->m_audioOutput->reset();
    }

    p->resetFade(0);
    p->resetBuffer();
}

bool AudioRenderer::isPaused() const
{
    return !p->m_isRunning;
}

bool AudioRenderer::isFading() const
{
    return p->m_fadeTimer.isActive();
}

void AudioRenderer::pause(bool paused, int fadeLength)
{
    p->resetFade(fadeLength);

    if(paused) {
        if(fadeLength == 0) {
            p->pause();
            return;
        }

        p->m_volumeChange = -(p->m_volume / p->m_fadeSteps);
    }
    else {
        p->pauseOutput(false);

        p->m_isRunning = true;
        p->m_writeTimer->start();

        if(fadeLength > 0) {
            p->m_volumeChange = std::abs(p->m_initialVolume - p->m_volume) / p->m_fadeSteps;
        }
        else {
            p->updateOutputVolume(p->m_initialVolume);
            return;
        }
    }

    p->m_fadeTimer.start(FadeInterval, this);
}

void AudioRenderer::queueBuffer(const AudioBuffer& buffer)
{
    p->m_bufferQueue.enqueue(buffer);
}

void AudioRenderer::updateOutput(const OutputCreator& output, const QString& device)
{
    auto newOutput = output();
    if(newOutput == p->m_audioOutput) {
        return;
    }

    const bool wasInitialised = p->m_audioOutput && p->m_audioOutput->initialised();

    if(wasInitialised) {
        p->m_audioOutput->uninit();
        QObject::disconnect(p->m_audioOutput.get(), nullptr, this, nullptr);
    }

    p->m_audioOutput = std::move(newOutput);
    if(!device.isEmpty()) {
        p->m_audioOutput->setDevice(device);
    }

    p->m_bufferPrefilled = false;
    QObject::connect(p->m_audioOutput.get(), &AudioOutput::stateChanged, this,
                     [this](const auto state) { p->outputStateChanged(state); });

    if(wasInitialised) {
        p->m_audioOutput->init(p->m_format);
    }
}

void AudioRenderer::updateDevice(const QString& device)
{
    if(!p->m_audioOutput) {
        return;
    }

    p->m_bufferPrefilled = false;

    if(p->m_audioOutput && p->m_audioOutput->initialised()) {
        p->m_audioOutput->uninit();
        p->m_audioOutput->setDevice(device);
        p->m_audioOutput->init(p->m_format);
    }
    else {
        p->m_audioOutput->setDevice(device);
    }
}

void AudioRenderer::updateVolume(double volume)
{
    p->m_initialVolume = volume;
    p->updateOutputVolume(volume);
}

void AudioRenderer::timerEvent(QTimerEvent* event)
{
    if(event->timerId() != p->m_fadeTimer.timerId()) {
        return;
    }

    if(p->m_currentFadeStep <= p->m_fadeSteps) {
        p->updateOutputVolume(std::clamp(p->m_volume + p->m_volumeChange, 0.0, 1.0));
        p->m_currentFadeStep++;
        return;
    }

    if(p->m_volumeChange < 0.0) {
        // Faded out
        p->pause();
    }
    else {
        p->updateOutputVolume(p->m_initialVolume);
    }

    p->m_fadeTimer.stop();
}
} // namespace Fooyin

#include "moc_audiorenderer.cpp"
