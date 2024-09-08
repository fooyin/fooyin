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

#include "internalcoresettings.h"

#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>
#include <core/engine/audiooutput.h>
#include <utils/settings/settingsmanager.h>
#include <utils/threadqueue.h>

#include <QBasicTimer>
#include <QDebug>
#include <QTimer>
#include <QTimerEvent>

#include <utility>

using namespace std::chrono_literals;

constexpr auto FadeInterval = 10;

namespace Fooyin {
AudioRenderer::AudioRenderer(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
    , m_volume{0.0}
    , m_gainScale{1.0}
    , m_bufferSize{0}
    , m_bufferPrefilled{false}
    , m_samplePos{0}
    , m_currentBufferOffset{0}
    , m_currentBufferResampled{false}
    , m_isRunning{false}
    , m_writeInterval{100}
    , m_fadeLength{0}
    , m_fadingOut{false}
    , m_flipFade{false}
    , m_fadeSteps{0}
    , m_currentFadeStep{0}
    , m_volumeChange{0.0}
    , m_fadeVolume{-1}
{
    setObjectName(QStringLiteral("Renderer"));

    m_settings->subscribe<Settings::Core::ReplayGainEnabled>(this, &AudioRenderer::calculateGain);
    m_settings->subscribe<Settings::Core::ReplayGainType>(this, &AudioRenderer::calculateGain);
    m_settings->subscribe<Settings::Core::ReplayGainPreAmp>(this, &AudioRenderer::calculateGain);
}

void AudioRenderer::init(const Track& track, const AudioFormat& format)
{
    m_format       = format;
    m_currentTrack = track;

    calculateGain();

    if(!m_audioOutput) {
        emit initialised(false);
    }

    if(m_audioOutput->initialised()) {
        m_audioOutput->uninit();
    }

    const bool success = initOutput();
    emit initialised(success);
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
    m_samplePos = 0;
    m_isRunning = false;
    m_writeTimer.stop();

    resetFade(0);
    resetBuffer();
    m_fadeVolume = -1;
}

void AudioRenderer::closeOutput()
{
    if(validOutputState()) {
        m_audioOutput->uninit();
    }
    emit outputClosed();
}

void AudioRenderer::drainOutput()
{
    if(validOutputState()) {
        m_audioOutput->drain();
    }
}

void AudioRenderer::reset()
{
    if(validOutputState()) {
        m_audioOutput->reset();
    }

    resetFade(0);
    resetBuffer();
    m_fadeVolume = -1;
}

void AudioRenderer::play()
{
    if(validOutputState()) {
        m_audioOutput->setPaused(false);
        m_audioOutput->setVolume(m_volume);
    }

    start();
}

void AudioRenderer::play(int fadeLength)
{
    resetFade(fadeLength);
    m_fadingOut = false;

    if(validOutputState()) {
        m_audioOutput->setPaused(false);
    }

    start();

    if(!m_flipFade && fadeLength > 0) {
        m_fadeVolume   = std::max(m_fadeVolume, 0.0);
        m_volumeChange = std::abs(m_fadeVolume - m_volume) / m_fadeSteps;
    }
    else {
        m_volumeChange = -m_volumeChange;
    }

    m_fadeTimer.start(FadeInterval, this);
}

void AudioRenderer::pause()
{
    pauseOutput();
}

void AudioRenderer::pause(int fadeLength)
{
    resetFade(fadeLength);
    m_fadingOut = true;

    if(!m_flipFade) {
        if(m_fadeVolume <= 0) {
            m_fadeVolume = m_volume;
        }
        m_volumeChange = -(m_fadeVolume / m_fadeSteps);
    }
    else {
        m_volumeChange = -m_volumeChange;
    }

    m_fadeTimer.start(FadeInterval, this);
}

void AudioRenderer::queueBuffer(const AudioBuffer& buffer)
{
    m_bufferQueue.emplace(buffer);
}

bool AudioRenderer::resetResampler()
{
    m_outputFormat = m_audioOutput->format();

    if(m_outputFormat.isValid() && m_outputFormat != m_format) {
        m_resampler
            = std::make_unique<FFmpegResampler>(m_format, m_outputFormat, m_format.durationForFrames(m_samplePos));
        if(!m_resampler->canResample()) {
            m_resampler.reset();
            return false;
        }
    }
    else {
        m_resampler.reset();
    }

    return true;
}

void AudioRenderer::updateOutput(const OutputCreator& output, const QString& device)
{
    auto newOutput = output();
    if(newOutput == m_audioOutput) {
        return;
    }

    if(m_audioOutput && m_audioOutput->initialised()) {
        m_audioOutput->uninit();
        QObject::disconnect(m_audioOutput.get(), nullptr, this, nullptr);
    }

    m_audioOutput = std::move(newOutput);
    if(!device.isEmpty()) {
        m_audioOutput->setDevice(device);
    }

    m_bufferPrefilled = false;
    QObject::connect(m_audioOutput.get(), &AudioOutput::stateChanged, this, &AudioRenderer::handleStateChanged);
}

void AudioRenderer::updateDevice(const QString& device)
{
    if(!m_audioOutput) {
        return;
    }

    m_bufferPrefilled = false;

    if(m_audioOutput->initialised()) {
        m_audioOutput->uninit();
    }

    m_audioOutput->setDevice(device);
}

void AudioRenderer::updateVolume(double volume)
{
    m_volume = volume;

    if(validOutputState()) {
        m_audioOutput->setVolume(m_volume);
    }
}

QString AudioRenderer::deviceError() const
{
    return m_lastDeviceError;
}

void AudioRenderer::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_writeTimer.timerId()) {
        writeNext();
    }
    else if(event->timerId() == m_fadeTimer.timerId()) {
        handleFading();
    }

    QObject::timerEvent(event);
}

void AudioRenderer::resetBuffer()
{
    m_bufferPrefilled        = false;
    m_samplePos              = 0;
    m_currentBufferOffset    = 0;
    m_currentBufferResampled = false;
    m_bufferQueue            = {};
    m_tempBuffer.reset();
}

void AudioRenderer::resetFade(int length)
{
    if(m_fadeTimer.isActive()) {
        m_fadeTimer.stop();
    }

    m_flipFade   = !m_flipFade && m_currentFadeStep > 0 && length > 0;
    m_fadeLength = length;

    if(!m_flipFade) {
        m_fadeSteps       = static_cast<int>(static_cast<double>(length) / FadeInterval);
        m_currentFadeStep = 0;
        m_volumeChange    = 0;
    }
}

void AudioRenderer::handleFading()
{
    auto updateOutputVolume = [this]() {
        if(validOutputState()) {
            m_audioOutput->setVolume(m_fadeSteps > 0 && m_fadeVolume >= 0 ? m_fadeVolume : m_volume);
        }
    };

    const auto currentStep = m_currentFadeStep;
    const bool inFade      = m_flipFade ? m_currentFadeStep-- >= 0 : m_currentFadeStep++ <= m_fadeSteps;

    if(inFade) {
        if(m_fadeLength >= 1000) {
            const auto step = static_cast<double>(currentStep) / m_fadeSteps;
            m_fadeVolume    = m_volume * ((1.0 + std::erf(3.0 * step - 1.5)) / 2.0);
            if(m_flipFade ^ m_fadingOut) {
                m_fadeVolume = m_volume - m_fadeVolume;
            }
        }
        else {
            m_fadeVolume += m_volumeChange;
        }

        m_fadeVolume = std::clamp(m_fadeVolume, 0.0, 1.0);
        updateOutputVolume();
        return;
    }

    if(m_volumeChange < 0.0) {
        // Faded out
        pauseOutput();
    }
    else {
        m_fadeVolume = -1;
    }

    m_currentFadeStep = 0;
    m_flipFade        = false;

    updateOutputVolume();
    m_fadeTimer.stop();
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

    if(!resetResampler()) {
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
        emit error(m_lastDeviceError);
        emit outputStateChanged(state);
        m_audioOutput->uninit();
        m_bufferPrefilled = false;
    }
}

void AudioRenderer::updateInterval()
{
    const auto interval
        = static_cast<int>(static_cast<double>(m_bufferSize) / m_outputFormat.sampleRate() * 1000 * 0.25);
    m_writeInterval = interval;
}

void AudioRenderer::calculateGain()
{
    m_gainScale = 1.0;

    if(!m_settings->value<Settings::Core::ReplayGainEnabled>()) {
        return;
    }

    float peak{0.0};
    const auto gainType = static_cast<ReplayGainType>(m_settings->value<Settings::Core::ReplayGainType>());
    switch(gainType) {
        case(ReplayGainType::Track):
            m_gainScale = std::pow(10.0, m_currentTrack.replayGainTrackGain() / 20.0);
            peak        = m_currentTrack.replayGainTrackPeak();
            break;
        case(ReplayGainType::Album):
            m_gainScale = std::pow(10.0, m_currentTrack.replayGainAlbumGain() / 20.0);
            peak        = m_currentTrack.replayGainAlbumPeak();
            break;
    }

    const auto preamp = static_cast<float>(m_settings->value<Settings::Core::ReplayGainPreAmp>());
    m_gainScale *= std::pow(10.0, preamp / 20.0);

    if(peak > 0.0) {
        // Prevent clipping
        m_gainScale = (m_gainScale * peak) > 1.0 ? (1.0 / peak) : m_gainScale;
    }
    // Clamp to +-20 dB
    m_gainScale = std::clamp(m_gainScale, 0.1, 10.0);
}

void AudioRenderer::pauseOutput()
{
    m_isRunning = false;
    m_writeTimer.stop();

    m_fadeVolume = -1;

    if(validOutputState()) {
        const auto state       = m_audioOutput->currentState();
        const uint64_t durLeft = m_outputFormat.durationForFrames(state.queuedSamples);

        emit paused(durLeft);
        drainOutput();
        m_audioOutput->setPaused(true);
        return;
    }

    emit paused(0);
}

void AudioRenderer::writeNext()
{
    if(!canWrite() || m_bufferQueue.empty()) {
        return;
    }

    const int freeSamples = m_audioOutput->currentState().freeSamples;

    const bool hasPrevWrite = (freeSamples == 0 && m_samplePos > 0);
    const bool bufferFilled = (freeSamples > 0 && renderAudio(freeSamples) == freeSamples);

    if(hasPrevWrite || bufferFilled) {
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

    while(m_isRunning && !m_bufferQueue.empty() && samplesBuffered < samples) {
        AudioBuffer& buffer = m_bufferQueue.front();

        if(!buffer.isValid()) {
            // End of file
            m_currentBufferOffset    = 0;
            m_currentBufferResampled = false;
            m_bufferQueue.pop();
            emit finished();
            return samplesBuffered;
        }

        if(m_resampler && !m_currentBufferResampled) {
            buffer = m_resampler->resample(buffer);

            m_currentBufferResampled = true;
        }

        const int bytesLeft = buffer.byteCount() - m_currentBufferOffset;

        if(bytesLeft <= 0) {
            m_currentBufferOffset    = 0;
            m_currentBufferResampled = false;
            emit bufferProcessed(buffer);
            m_bufferQueue.pop();
            continue;
        }

        const int sstride     = m_outputFormat.bytesPerFrame();
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

    m_tempBuffer.scale(m_gainScale);

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
    m_samplePos += samplesWritten;

    return samplesWritten;
}
} // namespace Fooyin

#include "moc_audiorenderer.cpp"
