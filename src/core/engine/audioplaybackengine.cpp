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

#include "audioplaybackengine.h"

#include "audioclock.h"
#include "audiorenderer.h"
#include "internalcoresettings.h"

#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <QBasicTimer>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>

using namespace std::chrono_literals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto BufferInterval   = 5ms;
constexpr auto PositionInterval = 50ms;
#else
constexpr auto BufferInterval   = 5;
constexpr auto PositionInterval = 50;
#endif

constexpr auto MaxDecodeLength = 200;

namespace Fooyin {
AudioPlaybackEngine::AudioPlaybackEngine(std::shared_ptr<AudioLoader> decoderProvider, SettingsManager* settings,
                                         QObject* parent)
    : AudioEngine{parent}
    , m_decoderProvider{std::move(decoderProvider)}
    , m_decoder{nullptr}
    , m_settings{settings}
    , m_status{TrackStatus::NoTrack}
    , m_state{PlaybackState::Stopped}
    , m_outputState{AudioOutput::State::None}
    , m_startPosition{0}
    , m_endPosition{0}
    , m_lastPosition{0}
    , m_totalBufferTime{0}
    , m_bufferLength{static_cast<uint64_t>(m_settings->value<Settings::Core::BufferLength>())}
    , m_duration{0}
    , m_volume{1.0}
    , m_ending{false}
    , m_decoding{false}
    , m_renderer{new AudioRenderer(this)}
    , m_fadeIntervals{m_settings->value<Settings::Core::Internal::FadingIntervals>().value<FadingIntervals>()}
{
    QObject::connect(m_renderer, &AudioRenderer::bufferProcessed, this, &AudioPlaybackEngine::onBufferProcessed);
    QObject::connect(m_renderer, &AudioRenderer::finished, this, &AudioPlaybackEngine::onRendererFinished);
    QObject::connect(m_renderer, &AudioRenderer::outputStateChanged, this, &AudioPlaybackEngine::handleOutputState);

    m_settings->subscribe<Settings::Core::BufferLength>(this, [this](int length) { m_bufferLength = length; });
    m_settings->subscribe<Settings::Core::Internal::FadingIntervals>(
        this, [this](const QVariant& fading) { m_fadeIntervals = fading.value<FadingIntervals>(); });
}

AudioPlaybackEngine::~AudioPlaybackEngine()
{
    stopWorkers();
    AudioPlaybackEngine::stop();
}

void AudioPlaybackEngine::seek(uint64_t pos)
{
    if(!m_decoder || !m_decoder->isSeekable()) {
        return;
    }

    resetWorkers();

    m_decoder->seek(pos + m_startPosition);
    m_clock.sync(pos);

    if(m_state == PlaybackState::Playing) {
        m_clock.setPaused(false);
        m_bufferTimer.start(BufferInterval, this);
        m_renderer->start();
    }
    else {
        updatePosition();
    }
}

void AudioPlaybackEngine::changeTrack(const Track& track)
{
    changeTrackStatus(TrackStatus::Loading);

    const Track prevTrack = std::exchange(m_currentTrack, track);

    if(!m_decoder || !m_decoder->supportedExtensions().contains(track.extension())) {
        m_decoder = m_decoderProvider->decoderForTrack(track);
        if(!m_decoder) {
            changeTrackStatus(TrackStatus::Unreadable);
            return;
        }
    }

    if(m_ending && track.filepath() == prevTrack.filepath() && m_endPosition == track.offset()) {
        // Multi-file track
        emit positionChanged(0);
        m_ending = false;
        m_clock.sync(0);
        setupDuration();
        changeTrackStatus(TrackStatus::Buffered);
        if(m_state == PlaybackState::Playing) {
            play();
        }
        return;
    }

    stopWorkers();

    emit positionChanged(0);

    m_ending = false;
    m_clock.setPaused(true);
    m_clock.sync();

    if(!track.isValid()) {
        changeTrackStatus(TrackStatus::Invalid);
        return;
    }

    changeTrackStatus(TrackStatus::Loading);

    if(!m_decoder->init(track.filepath(), {})) {
        changeTrackStatus(TrackStatus::Invalid);
        return;
    }

    if(!updateFormat(m_decoder->format())) {
        updateState(PlaybackState::Error);
        changeTrackStatus(TrackStatus::NoTrack);
        return;
    }

    changeTrackStatus(TrackStatus::Loaded);

    setupDuration();

    if(track.offset() > 0) {
        m_decoder->seek(track.offset());
    }

    if(m_state == PlaybackState::Playing) {
        playOutput();
    }
}

void AudioPlaybackEngine::play()
{
    if(m_status == TrackStatus::NoTrack || m_status == TrackStatus::Invalid || m_status == TrackStatus::Unreadable) {
        return;
    }

    if(m_status == TrackStatus::End && m_state == PlaybackState::Stopped) {
        seek(0);
        emit positionChanged(0);
    }

    if(m_state == PlaybackState::Stopped && m_status == TrackStatus::Buffered) {
        // Current track was previously stopped, so init again
        if(!m_decoder->init(m_currentTrack.filepath(), {})) {
            changeTrackStatus(TrackStatus::Invalid);
            return;
        }
    }

    playOutput();
}

void AudioPlaybackEngine::pause()
{
    if(m_status == TrackStatus::NoTrack || m_status == TrackStatus::Invalid) {
        return;
    }

    if(m_status == TrackStatus::End && m_state == PlaybackState::Stopped) {
        seek(0);
        emit positionChanged(0);
    }
    else {
        pauseOutput();
    }
}

void AudioPlaybackEngine::stop()
{
    if(m_state != PlaybackState::Stopped) {
        stopOutput();
    }
}

void AudioPlaybackEngine::setVolume(double volume)
{
    m_volume = volume;
    m_renderer->updateVolume(volume);
}

void AudioPlaybackEngine::setAudioOutput(const OutputCreator& output, const QString& device)
{
    const bool playing = m_state == PlaybackState::Playing;

    if(playing) {
        m_clock.setPaused(true);
        m_renderer->pause(true);
    }

    m_renderer->updateOutput(output, device);

    if(playing) {
        if(!m_renderer->init(m_format)) {
            changeTrackStatus(TrackStatus::NoTrack);
            return;
        }

        m_clock.setPaused(false);
        m_renderer->start();
        m_renderer->pause(false);
    }
}

void AudioPlaybackEngine::setOutputDevice(const QString& device)
{
    if(device.isEmpty()) {
        return;
    }

    const bool playing = m_state == PlaybackState::Playing;

    if(playing) {
        m_clock.setPaused(true);
        m_renderer->pause(true);
    }

    m_renderer->updateDevice(device);

    if(playing) {
        if(!m_renderer->init(m_format)) {
            changeTrackStatus(TrackStatus::NoTrack);
            return;
        }

        m_clock.setPaused(false);
        startPlayback();
        m_renderer->pause(false);
    }
}

void AudioPlaybackEngine::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_bufferTimer.timerId()) {
        readNextBuffer();
    }
    else if(event->timerId() == m_posTimer.timerId()) {
        updatePosition();
    }

    QObject::timerEvent(event);
}

void AudioPlaybackEngine::resetWorkers()
{
    m_bufferTimer.stop();
    m_clock.setPaused(true);
    m_renderer->reset();
    m_totalBufferTime = 0;
}

void AudioPlaybackEngine::stopWorkers(bool full)
{
    m_bufferTimer.stop();
    m_posTimer.stop();
    m_clock.setPaused(true);
    m_clock.sync();
    m_decoding = false;
    m_renderer->stop();
    if(full) {
        m_renderer->closeOutput();
        m_outputState = AudioOutput::State::Disconnected;
    }
    if(m_decoder && m_state != PlaybackState::Stopped) {
        m_decoder->stop();
    }
    m_totalBufferTime = 0;
}

void AudioPlaybackEngine::handleOutputState(AudioOutput::State outState)
{
    m_outputState = outState;
    if(m_outputState == AudioOutput::State::Disconnected) {
        pause();
    }
    else if(m_outputState == AudioOutput::State::Error) {
        stop();
        if(!m_renderer->deviceError().isEmpty()) {
            emit deviceError(m_renderer->deviceError());
        }
    }
}

void AudioPlaybackEngine::updateState(PlaybackState newState)
{
    if(std::exchange(m_state, newState) != m_state) {
        emit stateChanged(newState);
    }
    m_clock.setPaused(newState != PlaybackState::Playing);
}

TrackStatus AudioPlaybackEngine::changeTrackStatus(TrackStatus newStatus)
{
    auto prevStatus = std::exchange(m_status, newStatus);
    if(prevStatus != m_status) {
        emit trackStatusChanged(m_status);
    }
    return prevStatus;
}

void AudioPlaybackEngine::setupDuration()
{
    m_duration = m_currentTrack.duration();
    if(m_duration == 0) {
        // Handle cases without a total number of samples
        m_duration = std::numeric_limits<uint64_t>::max();
    }
    m_startPosition = m_currentTrack.offset();
    m_endPosition   = m_startPosition + m_duration;
    m_lastPosition  = m_startPosition;
};

bool AudioPlaybackEngine::updateFormat(const AudioFormat& nextFormat)
{
    const auto prevFormat = std::exchange(m_format, nextFormat);

    if(m_settings->value<Settings::Core::GaplessPlayback>() && prevFormat == m_format
       && m_state != PlaybackState::Paused) {
        return true;
    }

    if(!m_renderer->init(m_format)) {
        m_format = {};
        return false;
    }

    return true;
}

void AudioPlaybackEngine::startPlayback()
{
    if(m_decoder && !m_decoding) {
        m_decoding = true;
        m_decoder->start();
    }
    m_bufferTimer.start(BufferInterval, this);
    m_renderer->start();
}

void AudioPlaybackEngine::playOutput()
{
    if(!m_decoder) {
        return;
    }

    if(m_outputState == AudioOutput::State::Disconnected) {
        if(m_renderer->init(m_format)) {
            m_outputState = AudioOutput::State::None;
        }
        else {
            updateState(PlaybackState::Error);
            return;
        }
    }

    startPlayback();

    if(m_state == PlaybackState::Stopped && m_currentTrack.offset() > 0) {
        m_decoder->seek(m_currentTrack.offset());
    }

    updateState(PlaybackState::Playing);

    const bool canFade = m_settings->value<Settings::Core::Internal::EngineFading>()
                      && (m_state == PlaybackState::Paused || m_renderer->isFading());
    const int fadeInterval = canFade ? m_fadeIntervals.inPauseStop : 0;

    m_renderer->pause(false, fadeInterval);
    changeTrackStatus(TrackStatus::Buffered);
    m_posTimer.start(PositionInterval, Qt::PreciseTimer, this);
}

void AudioPlaybackEngine::pauseOutput()
{
    auto delayedPause = [this]() {
        m_bufferTimer.stop();
        updateState(PlaybackState::Paused);
        m_clock.setPaused(true);
    };

    QObject::connect(m_renderer, &AudioRenderer::paused, this, delayedPause, Qt::SingleShotConnection);

    const int fadeInterval
        = m_settings->value<Settings::Core::Internal::EngineFading>() ? m_fadeIntervals.outPauseStop : 0;
    m_renderer->pause(true, fadeInterval);
}

void AudioPlaybackEngine::stopOutput()
{
    auto delayedStop = [this]() {
        stopWorkers(true);
    };

    const bool canFade
        = m_settings->value<Settings::Core::Internal::EngineFading>() && m_fadeIntervals.outPauseStop > 0;
    if(canFade) {
        QObject::connect(m_renderer, &AudioRenderer::paused, this, delayedStop, Qt::SingleShotConnection);
        m_renderer->pause(true, m_fadeIntervals.outPauseStop);
    }
    else {
        stopWorkers(true);
    }

    updateState(PlaybackState::Stopped);
    m_posTimer.stop();
    m_lastPosition = 0;
    emit positionChanged(0);
}

void AudioPlaybackEngine::readNextBuffer()
{
    if(!m_decoder || m_totalBufferTime >= m_bufferLength) {
        return;
    }

    const auto bytesToEnd = static_cast<size_t>(m_format.bytesForDuration(m_endPosition - m_lastPosition));
    const auto bytesLeft
        = std::min(bytesToEnd, static_cast<size_t>(m_format.bytesForDuration(m_bufferLength - m_totalBufferTime)));
    const auto maxBytes = std::min(bytesLeft, static_cast<size_t>(m_format.bytesForDuration(MaxDecodeLength)));

    const auto buffer = m_decoder->readBuffer(maxBytes);
    if(buffer.isValid()) {
        m_totalBufferTime += buffer.duration();
        m_renderer->queueBuffer(buffer);
    }

    if(!buffer.isValid() || (m_currentTrack.hasCue() && buffer.endTime() >= m_endPosition)) {
        m_bufferTimer.stop();
        m_renderer->queueBuffer({});
        m_ending = true;
        emit trackAboutToFinish();
    }
}

void AudioPlaybackEngine::updatePosition()
{
    const auto currentPosition = m_startPosition + m_clock.currentPosition();
    if(std::exchange(m_lastPosition, currentPosition) != m_lastPosition) {
        emit positionChanged(m_lastPosition - m_startPosition);
    }

    if(m_currentTrack.hasCue() && m_lastPosition >= m_endPosition) {
        m_clock.setPaused(true);
        m_clock.sync(m_duration);
        changeTrackStatus(TrackStatus::End);
    }
}

void AudioPlaybackEngine::onBufferProcessed(const AudioBuffer& buffer)
{
    m_totalBufferTime -= buffer.duration();
}

void AudioPlaybackEngine::onRendererFinished()
{
    if(m_currentTrack.hasCue()) {
        return;
    }

    m_clock.setPaused(true);
    m_clock.sync(m_duration);

    changeTrackStatus(TrackStatus::End);
}
} // namespace Fooyin

#include "moc_audioplaybackengine.cpp"
