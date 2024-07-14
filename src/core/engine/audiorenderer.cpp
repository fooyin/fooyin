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
AudioRenderer::AudioRenderer(QObject* parent)
    : QObject{parent}
    , m_volume{0.0}
    , m_bufferSize{0}
    , m_bufferPrefilled{false}
    , m_totalSamplesWritten{0}
    , m_currentBufferOffset{0}
    , m_isRunning{false}
    , m_writeInterval{100}
    , m_fadeLength{0}
    , m_fadeSteps{0}
    , m_currentFadeStep{0}
    , m_volumeChange{0.0}
    , m_initialVolume{0.0}
{
    setObjectName(QStringLiteral("Renderer"));
}

AudioRenderer::~AudioRenderer()
{
    if(m_audioOutput && m_audioOutput->initialised()) {
        m_audioOutput->uninit();
    }
}

bool AudioRenderer::init(const AudioFormat& format)
{
    m_format = format;

    if(!m_audioOutput) {
        return false;
    }

    if(m_audioOutput->initialised()) {
        m_audioOutput->uninit();
    }

    return initOutput();
}

void AudioRenderer::start()
{
    if(std::exchange(m_isRunning, true)) {
        return;
    }

    m_writeTimer.start(m_writeInterval, Qt::PreciseTimer, this);
}

void AudioRenderer::stop()
{
    m_isRunning = false;
    m_writeTimer.stop();

    resetFade(0);
    resetBuffer();
}

void AudioRenderer::closeOutput()
{
    if(m_audioOutput->initialised()) {
        m_audioOutput->uninit();
    }
}

void AudioRenderer::reset()
{
    if(m_audioOutput && m_audioOutput->initialised()) {
        m_audioOutput->reset();
    }

    resetFade(0);
    resetBuffer();
}

bool AudioRenderer::isPaused() const
{
    return !m_isRunning;
}

bool AudioRenderer::isFading() const
{
    return m_fadeTimer.isActive();
}

void AudioRenderer::pause(bool paused, int fadeLength)
{
    resetFade(fadeLength);

    if(paused) {
        if(fadeLength == 0) {
            pause();
            return;
        }

        m_volumeChange = -(m_volume / m_fadeSteps);
    }
    else {
        if(validOutputState()) {
            m_audioOutput->setPaused(false);
        }
        start();

        if(fadeLength > 0) {
            m_volumeChange = std::abs(m_initialVolume - m_volume) / m_fadeSteps;
        }
        else {
            updateOutputVolume(m_initialVolume);
        }
    }

    m_fadeTimer.start(FadeInterval, this);
}

void AudioRenderer::queueBuffer(const AudioBuffer& buffer)
{
    m_bufferQueue.emplace(buffer);
}

void AudioRenderer::updateOutput(const OutputCreator& output, const QString& device)
{
    auto newOutput = output();
    if(newOutput == m_audioOutput) {
        return;
    }

    const bool wasInitialised = m_audioOutput && m_audioOutput->initialised();

    if(wasInitialised) {
        m_audioOutput->uninit();
        QObject::disconnect(m_audioOutput.get(), nullptr, this, nullptr);
    }

    m_audioOutput = std::move(newOutput);
    if(!device.isEmpty()) {
        m_audioOutput->setDevice(device);
    }

    m_bufferPrefilled = false;
    QObject::connect(m_audioOutput.get(), &AudioOutput::stateChanged, this, &AudioRenderer::handleStateChanged);

    if(wasInitialised) {
        m_audioOutput->init(m_format);
    }
}

void AudioRenderer::updateDevice(const QString& device)
{
    if(!m_audioOutput) {
        return;
    }

    m_bufferPrefilled = false;

    if(m_audioOutput && m_audioOutput->initialised()) {
        m_audioOutput->uninit();
        m_audioOutput->setDevice(device);
        m_audioOutput->init(m_format);
    }
    else {
        m_audioOutput->setDevice(device);
    }
}

void AudioRenderer::updateVolume(double volume)
{
    m_initialVolume = volume;
    updateOutputVolume(volume);
}

QString AudioRenderer::deviceError() const
{
    return m_lastDeviceError;
}

void AudioRenderer::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_writeTimer.timerId()) {
        writeNext();
        return;
    }
    if(event->timerId() != m_fadeTimer.timerId()) {
        return;
    }

    if(m_currentFadeStep <= m_fadeSteps) {
        updateOutputVolume(std::clamp(m_volume + m_volumeChange, 0.0, 1.0));
        m_currentFadeStep++;
        return;
    }

    if(m_volumeChange < 0.0) {
        // Faded out
        pause();
    }
    else {
        updateOutputVolume(m_initialVolume);
    }

    m_fadeTimer.stop();
}

void AudioRenderer::resetBuffer()
{
    m_bufferPrefilled     = false;
    m_totalSamplesWritten = 0;
    m_currentBufferOffset = 0;
    m_bufferQueue         = {};
    m_tempBuffer.reset();
}

void AudioRenderer::resetFade(int length)
{
    if(m_fadeTimer.isActive()) {
        m_fadeTimer.stop();
    }

    m_volumeChange    = 0;
    m_currentFadeStep = 0;
    m_fadeLength      = length;
    m_fadeSteps       = static_cast<int>(static_cast<double>(m_fadeLength) / FadeInterval);
}

bool AudioRenderer::canWrite() const
{
    return m_isRunning && m_audioOutput->initialised();
}

bool AudioRenderer::initOutput()
{
    if(!m_audioOutput->init(m_format)) {
        return false;
    }

    m_audioOutput->setVolume(m_volume);
    m_bufferSize = m_audioOutput->bufferSize();
    updateInterval();

    return true;
}

bool AudioRenderer::validOutputState() const
{
    return m_audioOutput && m_audioOutput->initialised() && m_audioOutput->error().isEmpty();
}

void AudioRenderer::handleStateChanged(AudioOutput::State state)
{
    if(state == AudioOutput::State::Disconnected || state == AudioOutput::State::Error) {
        m_lastDeviceError = m_audioOutput->error();
        emit outputStateChanged(state);
        m_audioOutput->uninit();
        m_bufferPrefilled = false;
    }
}

void AudioRenderer::updateOutputVolume(double newVolume)
{
    m_volume = newVolume;

    if(validOutputState()) {
        m_audioOutput->setVolume(m_volume);
    }
}

void AudioRenderer::updateInterval()
{
    const auto interval = static_cast<int>(static_cast<double>(m_bufferSize) / m_format.sampleRate() * 1000 * 0.25);
    m_writeInterval     = interval;
}

void AudioRenderer::pause()
{
    m_isRunning = false;
    m_writeTimer.stop();

    updateOutputVolume(0.0);

    if(validOutputState()) {
        m_audioOutput->drain();
    }
    emit paused();

    if(validOutputState()) {
        m_audioOutput->setPaused(true);
    }
}

void AudioRenderer::writeNext()
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

int AudioRenderer::writeAudioSamples(int samples)
{
    m_tempBuffer.clear();

    int samplesBuffered{0};

    const int sstride = m_format.bytesPerFrame();

    while(m_isRunning && !m_bufferQueue.empty() && samplesBuffered < samples) {
        const AudioBuffer& buffer = m_bufferQueue.front();

        if(!buffer.isValid()) {
            // End of file
            m_currentBufferOffset = 0;
            m_bufferQueue.pop();
            emit finished();
            return samplesBuffered;
        }

        const int bytesLeft = buffer.byteCount() - m_currentBufferOffset;

        if(bytesLeft <= 0) {
            m_currentBufferOffset = 0;
            emit bufferProcessed(buffer);
            m_bufferQueue.pop();
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

int AudioRenderer::renderAudio(int samples)
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
} // namespace Fooyin

#include "moc_audiorenderer.cpp"
