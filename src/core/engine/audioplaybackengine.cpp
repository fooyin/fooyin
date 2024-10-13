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
#include <QFile>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>

using namespace std::chrono_literals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto BufferInterval   = 5ms;
constexpr auto PositionInterval = 50ms;
constexpr auto BitrateInterval  = 200ms;
#else
constexpr auto BufferInterval   = 5;
constexpr auto PositionInterval = 50;
constexpr auto BitrateInterval  = 200;
#endif

constexpr auto MaxDecodeLength = 100;

namespace Fooyin {
AudioPlaybackEngine::AudioPlaybackEngine(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                                         QObject* parent)
    : AudioEngine{parent}
    , m_audioLoader{std::move(audioLoader)}
    , m_settings{settings}
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
    , m_updatingTrack{false}
    , m_pauseNextTrack{false}
    , m_decoder{nullptr}
    , m_nextDecoder{nullptr}
    , m_outputThread{new QThread(this)}
    , m_renderer{settings}
    , m_fadeIntervals{m_settings->value<Settings::Core::Internal::FadingIntervals>().value<FadingIntervals>()}
{
    m_renderer.moveToThread(m_outputThread);

    QObject::connect(&m_renderer, &AudioRenderer::requestOutputReload, this, &AudioPlaybackEngine::reloadOutput);
    QObject::connect(&m_renderer, &AudioRenderer::bufferProcessed, this, &AudioPlaybackEngine::onBufferProcessed);
    QObject::connect(&m_renderer, &AudioRenderer::finished, this, &AudioPlaybackEngine::onRendererFinished);
    QObject::connect(&m_renderer, &AudioRenderer::outputStateChanged, this, &AudioPlaybackEngine::handleOutputState);
    QObject::connect(&m_renderer, &AudioRenderer::error, this, &AudioPlaybackEngine::deviceError);

    m_settings->subscribe<Settings::Core::BufferLength>(this, [this](int length) { m_bufferLength = length; });
    m_settings->subscribe<Settings::Core::Internal::FadingIntervals>(
        this, [this](const QVariant& fading) { m_fadeIntervals = fading.value<FadingIntervals>(); });

    m_outputThread->start();
}

AudioPlaybackEngine::~AudioPlaybackEngine()
{
    stopWorkers(true);

    m_outputThread->quit();
    m_outputThread->wait();
}

void AudioPlaybackEngine::loadTrack(const Track& track)
{
    QObject::disconnect(this, &AudioEngine::trackStatusChanged, this, nullptr);

    if(m_updatingTrack) {
        m_updatingTrack = false;
        return;
    }

    if(!track.isValid()) {
        updateTrackStatus(TrackStatus::Invalid);
        return;
    }

    qCDebug(ENGINE) << "Loading track:" << track.filenameExt();

    std::optional<AudioFormat> format;

    if(m_nextDecoder && m_nextTrack == track) {
        stopWorkers();
        m_ending = false;
        m_clock.setPaused(true);
        m_clock.sync();

        format = loadPreparedTrack();
    }
    else {
        resetNextTrack();

        updateTrackStatus(TrackStatus::Loading);

        m_decoder = m_audioLoader->decoderForTrack(track);
        if(!m_decoder) {
            updateTrackStatus(TrackStatus::Unreadable);
            return;
        }

        const Track prevTrack = std::exchange(m_currentTrack, track);

        if(m_ending && track.filepath() == prevTrack.filepath() && m_endPosition == track.offset()) {
            // Multi-track file
            emit positionChanged(m_currentTrack, 0);
            m_ending = false;
            m_clock.sync(0);
            setupDuration();
            updateTrackStatus(TrackStatus::Buffered);
            if(playbackState() == PlaybackState::Playing) {
                play();
            }
            return;
        }

        stopWorkers();
        m_ending = false;
        m_clock.setPaused(true);
        m_clock.sync();

        if(!checkOpenSource()) {
            updateTrackStatus(TrackStatus::Invalid);
            return;
        }
        m_source.filepath = track.filepath();

        format = m_decoder->init(m_source, track, AudioDecoder::UpdateTracks);
        if(!format) {
            updateTrackStatus(TrackStatus::Invalid);
            return;
        }
    }

    if(!format) {
        return;
    }

    if(m_decoder->trackHasChanged()) {
        m_updatingTrack = true;
        m_currentTrack  = m_decoder->changedTrack();
        emit trackChanged(m_currentTrack);
    }

    updateFormat(format.value(), [this, track](bool success) {
        if(!success) {
            updateState(PlaybackState::Error);
            updateTrackStatus(TrackStatus::NoTrack);
        }
        else {
            setupDuration();
            updateTrackStatus(TrackStatus::Loaded);
            if(track.offset() > 0) {
                m_decoder->seek(track.offset());
            }

            if(const auto pendingState = std::exchange(m_pendingState, {})) {
                switch(pendingState.value()) {
                    case(AudioEngine::PlaybackState::Stopped):
                        stop();
                        break;
                    case(AudioEngine::PlaybackState::Playing):
                        play();
                        break;
                    case(AudioEngine::PlaybackState::Paused):
                        pause();
                        break;
                    default:
                        break;
                }
            }
        }
    });
}

void AudioPlaybackEngine::prepareNextTrack(const Track& track)
{
    resetNextTrack();

    if(track.isValid()) {
        return;
    }

    if(track.filepath() == m_currentTrack.filepath()) {
        // Same file - No need to prepare
        return;
    }

    m_nextDecoder = m_audioLoader->decoderForTrack(track);
    if(!m_nextDecoder) {
        return;
    }

    if(!track.isInArchive()) {
        m_nextFile = std::make_unique<QFile>(track.filepath());
        if(!m_nextFile->open(QIODevice::ReadOnly)) {
            return;
        }
        m_nextSource.device   = m_nextFile.get();
        m_nextSource.filepath = track.filepath();
    }

    m_nextTrack = track;

    const auto format = m_nextDecoder->init(m_nextSource, track, AudioDecoder::UpdateTracks);
    if(!format) {
        resetNextTrack();
        return;
    }

    m_nextFormat = format.value();
}

void AudioPlaybackEngine::play()
{
    if(trackStatus() == TrackStatus::Loading) {
        m_pendingState = PlaybackState::Playing;
        return;
    }

    if(!trackIsValid()) {
        return;
    }

    if(!checkReadyToDecode()) {
        return;
    }

    auto runOutput = [this]() {
        QObject::disconnect(&m_renderer, &AudioRenderer::paused, this, nullptr);

        if(!m_decoder) {
            return;
        }

        if(!m_decoding) {
            m_decoding = true;
            m_decoder->start();
        }

        if(m_pendingSeek) {
            resetWorkers();
            m_decoder->seek(m_pendingSeek.value());
            m_pendingSeek = {};
        }

        m_bufferTimer.start(BufferInterval, this);
        QMetaObject::invokeMethod(&m_renderer, &AudioRenderer::start);

        if(playbackState() == PlaybackState::Stopped && m_currentTrack.offset() > 0) {
            m_decoder->seek(m_currentTrack.offset());
        }

        const bool canFade = m_settings->value<Settings::Core::Internal::EngineFading>()
                          && (playbackState() == PlaybackState::Paused || isFading());
        const int fadeLength = canFade ? calculateFadeLength(m_fadeIntervals.inPauseStop) : 0;

        QMetaObject::invokeMethod(&m_renderer, [this, fadeLength]() { m_renderer.play(fadeLength); });
        updateTrackStatus(TrackStatus::Buffered);

        m_posTimer.start(PositionInterval, Qt::PreciseTimer, this);
        m_bitrateTimer.start(BitrateInterval, Qt::PreciseTimer, this);

        updateState(PlaybackState::Playing);
    };

    if(m_outputState == AudioOutput::State::Disconnected) {
        QObject::connect(
            &m_renderer, &AudioRenderer::initialised, this,
            [this, runOutput](bool success) {
                if(success) {
                    m_outputState = AudioOutput::State::None;
                    runOutput();
                }
                else {
                    updateState(PlaybackState::Error);
                }
            },
            Qt::SingleShotConnection);
        QMetaObject::invokeMethod(&m_renderer, [this]() { m_renderer.init(m_currentTrack, m_format); });
    }
    else {
        runOutput();
    }
}

void AudioPlaybackEngine::pause()
{
    if(playbackState() == PlaybackState::Stopped) {
        return;
    }

    if(trackStatus() == TrackStatus::Loading) {
        m_pendingState = PlaybackState::Paused;
        return;
    }

    if(!trackIsValid()) {
        return;
    }

    if(trackStatus() == TrackStatus::End && playbackState() == PlaybackState::Stopped) {
        seek(0);
        emit positionChanged(m_currentTrack, 0);
        return;
    }

    const auto pauseEngine = [this](const uint64_t delay) {
        QTimer::singleShot(delay, this, [this]() {
            m_bufferTimer.stop();
            if(playbackState() != PlaybackState::Stopped) {
                updateState(PlaybackState::Paused);
            }
        });
    };

    QObject::disconnect(m_pausedConnection);
    m_pausedConnection
        = QObject::connect(&m_renderer, &AudioRenderer::paused, this, pauseEngine, Qt::SingleShotConnection);

    const int fadeLength = (m_settings->value<Settings::Core::Internal::EngineFading>() && m_volume > 0.0)
                             ? calculateFadeLength(m_fadeIntervals.outPauseStop)
                             : 0;
    if(fadeLength > 0) {
        if(fadeLength < m_fadeIntervals.outPauseStop) {
            m_pauseNextTrack = true;
        }
        QMetaObject::invokeMethod(&m_renderer, [this, fadeLength]() { m_renderer.pause(fadeLength); });
        updateState(PlaybackState::FadingOut);
    }
    else {
        QMetaObject::invokeMethod(&m_renderer, [this]() { m_renderer.pause(); });
    }
}

void AudioPlaybackEngine::stop()
{
    if(playbackState() == PlaybackState::Stopped) {
        return;
    }

    if(trackStatus() == TrackStatus::Loading) {
        m_pendingState = PlaybackState::Stopped;
        return;
    }

    auto stopEngine = [this]() {
        AudioPlaybackEngine::updateState(PlaybackState::Stopped);
        QObject::connect(&m_renderer, &AudioRenderer::outputClosed, this, &AudioPlaybackEngine::finished,
                         Qt::SingleShotConnection);
        stopWorkers(true);
    };

    const bool canFade = playbackState() != PlaybackState::Paused
                      && m_settings->value<Settings::Core::Internal::EngineFading>() && m_fadeIntervals.outPauseStop > 0
                      && m_volume > 0.0;
    if(canFade) {
        const int fadeLength = calculateFadeLength(m_fadeIntervals.outPauseStop);
        if(fadeLength > 0 && fadeLength < m_fadeIntervals.outPauseStop) {
            m_pauseNextTrack = true;
        }

        QObject::disconnect(m_pausedConnection);
        m_pausedConnection
            = QObject::connect(&m_renderer, &AudioRenderer::paused, this, stopEngine, Qt::SingleShotConnection);
        QMetaObject::invokeMethod(&m_renderer, [this, fadeLength]() { m_renderer.pause(fadeLength); });

        AudioPlaybackEngine::updateState(PlaybackState::FadingOut);
    }
    else {
        stopEngine();
    }

    m_posTimer.stop();
    m_bitrateTimer.stop();
    m_lastPosition = 0;
    emit positionChanged(m_currentTrack, 0);
}

void AudioPlaybackEngine::seek(uint64_t pos)
{
    if(!m_decoder || !m_decoder->isSeekable()) {
        return;
    }

    if(playbackState() != PlaybackState::Playing || m_pendingSeek) {
        m_pendingSeek = pos + m_startPosition;
        m_clock.setPaused(true);
        m_clock.sync(pos);
        updatePosition();
        return;
    }

    resetWorkers();
    m_decoder->seek(pos + m_startPosition);
    m_clock.sync(pos);

    if(playbackState() == PlaybackState::Playing) {
        m_clock.setPaused(false);
        m_bufferTimer.start(BufferInterval, this);
        QMetaObject::invokeMethod(&m_renderer, &AudioRenderer::start);
    }
    else {
        updatePosition();
    }
}

void AudioPlaybackEngine::setVolume(double volume)
{
    m_volume = volume;
    QMetaObject::invokeMethod(&m_renderer, [this, volume]() { m_renderer.updateVolume(volume); });
}

void AudioPlaybackEngine::setAudioOutput(const OutputCreator& output, const QString& device)
{
    const bool outputActive = playbackState() != PlaybackState::Stopped;

    if(outputActive) {
        m_clock.setPaused(true);
        QMetaObject::invokeMethod(&m_renderer, qOverload<>(&AudioRenderer::pause));
    }

    QMetaObject::invokeMethod(&m_renderer, [this, output, device]() { m_renderer.updateOutput(output, device); });

    if(outputActive) {
        QObject::connect(
            &m_renderer, &AudioRenderer::initialised, this,
            [this](bool success) {
                if(!success) {
                    updateTrackStatus(TrackStatus::NoTrack);
                }
                else if(playbackState() == PlaybackState::Playing) {
                    m_clock.setPaused(false);
                    QMetaObject::invokeMethod(&m_renderer, qOverload<>(&AudioRenderer::play));
                }
            },
            Qt::SingleShotConnection);
        QMetaObject::invokeMethod(&m_renderer, [this]() { m_renderer.init(m_currentTrack, m_format); });
    }
}

void AudioPlaybackEngine::setOutputDevice(const QString& device)
{
    if(device.isEmpty()) {
        return;
    }

    const bool outputActive = playbackState() != PlaybackState::Stopped;

    if(outputActive) {
        m_clock.setPaused(true);
        QMetaObject::invokeMethod(&m_renderer, qOverload<>(&AudioRenderer::pause));
    }

    QMetaObject::invokeMethod(&m_renderer, [this, device]() { m_renderer.updateDevice(device); });

    if(outputActive) {
        QObject::connect(&m_renderer, &AudioRenderer::initialised, this, [this](bool success) {
            if(!success) {
                updateTrackStatus(TrackStatus::NoTrack);
            }
            else if(playbackState() == PlaybackState::Playing) {
                m_clock.setPaused(false);
                QMetaObject::invokeMethod(&m_renderer, qOverload<>(&AudioRenderer::play));
            }
        });
        QMetaObject::invokeMethod(&m_renderer, [this]() { m_renderer.init(m_currentTrack, m_format); });
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
    else if(event->timerId() == m_bitrateTimer.timerId()) {
        updateBitrate();
    }

    QObject::timerEvent(event);
}

AudioEngine::PlaybackState AudioPlaybackEngine::updateState(AudioEngine::PlaybackState state)
{
    const auto prevState = AudioEngine::updateState(state);
    m_clock.setPaused(state != PlaybackState::Playing && !isFading());
    return prevState;
}

void AudioPlaybackEngine::resetNextTrack()
{
    m_nextDecoder = nullptr;
    m_nextTrack   = {};
    m_nextSource  = {};
    m_nextFormat  = {};
}

AudioFormat AudioPlaybackEngine::loadPreparedTrack()
{
    m_decoder      = std::exchange(m_nextDecoder, nullptr);
    m_currentTrack = std::exchange(m_nextTrack, {});
    m_source       = std::exchange(m_nextSource, {});
    m_file         = std::move(m_nextFile);

    AudioFormat format = std::exchange(m_nextFormat, {});
    return format;
}

void AudioPlaybackEngine::resetWorkers()
{
    m_bufferTimer.stop();
    m_clock.setPaused(true);
    QMetaObject::invokeMethod(&m_renderer, &AudioRenderer::reset);
    m_totalBufferTime = 0;
}

void AudioPlaybackEngine::stopWorkers(bool full)
{
    m_bufferTimer.stop();
    m_posTimer.stop();
    m_bitrateTimer.stop();

    m_clock.setPaused(true);
    m_clock.sync();

    m_pendingSeek = {};
    m_decoding    = false;

    QMetaObject::invokeMethod(&m_renderer, &AudioRenderer::stop);

    if(full) {
        QMetaObject::invokeMethod(&m_renderer, &AudioRenderer::closeOutput);
        m_outputState = AudioOutput::State::Disconnected;
    }
    if(m_decoder && (full || playbackState() != PlaybackState::Stopped)) {
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
    }
}

void AudioPlaybackEngine::reloadOutput()
{
    const bool outputActive = playbackState() != PlaybackState::Stopped;

    if(outputActive) {
        m_clock.setPaused(true);
        QMetaObject::invokeMethod(&m_renderer, qOverload<>(&AudioRenderer::pause));

        QObject::connect(
            &m_renderer, &AudioRenderer::initialised, this,
            [this](bool success) {
                if(!success) {
                    updateTrackStatus(TrackStatus::NoTrack);
                }
                else if(playbackState() == PlaybackState::Playing) {
                    QMetaObject::invokeMethod(&m_renderer, qOverload<>(&AudioRenderer::play));
                    m_clock.setPaused(false);
                }
            },
            Qt::SingleShotConnection);
        QMetaObject::invokeMethod(&m_renderer, [this]() { m_renderer.init(m_currentTrack, m_format); });
    }
}

bool AudioPlaybackEngine::checkOpenSource()
{
    if(m_currentTrack.isInArchive()) {
        return true;
    }

    m_file = std::make_unique<QFile>(m_currentTrack.filepath());
    if(!m_file->open(QIODevice::ReadOnly)) {
        updateTrackStatus(TrackStatus::Invalid);
        return false;
    }
    m_source.device = m_file.get();
    return true;
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

void AudioPlaybackEngine::updateFormat(const AudioFormat& nextFormat, const std::function<void(bool)>& callback)
{
    const auto prevFormat = std::exchange(m_format, nextFormat);

    if(m_settings->value<Settings::Core::GaplessPlayback>() && prevFormat == m_format
       && playbackState() != PlaybackState::Paused && m_outputState != AudioOutput::State::Error) {
        callback(true);
        return;
    }

    QObject::connect(
        &m_renderer, &AudioRenderer::initialised, this,
        [this, callback](bool success) {
            if(!success) {
                m_format = {};
            }
            callback(success);
        },
        Qt::SingleShotConnection);
    QMetaObject::invokeMethod(&m_renderer, [this]() { m_renderer.init(m_currentTrack, m_format); });
}

bool AudioPlaybackEngine::checkReadyToDecode()
{
    if(playbackState() != PlaybackState::Stopped) {
        return true;
    }

    if(!m_decoder || !trackCanPlay()) {
        return false;
    }

    if(!checkOpenSource()) {
        return false;
    }

    if(!m_decoder->init(m_source, m_currentTrack, AudioDecoder::UpdateTracks)) {
        updateTrackStatus(TrackStatus::Invalid);
        return false;
    }

    return true;
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
        QMetaObject::invokeMethod(&m_renderer, [this, buffer]() { m_renderer.queueBuffer(buffer); });
    }

    if(!buffer.isValid() || (m_currentTrack.hasCue() && buffer.endTime() >= m_endPosition)) {
        m_bufferTimer.stop();
        QMetaObject::invokeMethod(&m_renderer, [this]() { m_renderer.queueBuffer({}); });
        m_ending = true;
        emit trackAboutToFinish();
    }
}

void AudioPlaybackEngine::updatePosition()
{
    const auto currentPosition = m_startPosition + m_clock.currentPosition();
    if(std::exchange(m_lastPosition, currentPosition) != m_lastPosition) {
        emit positionChanged(m_currentTrack, m_lastPosition - m_startPosition);
    }

    if(m_currentTrack.hasCue() && m_lastPosition >= m_endPosition) {
        m_clock.setPaused(true);
        m_clock.sync(m_duration);
        updateTrackStatus(TrackStatus::End);
    }
}

void AudioPlaybackEngine::updateBitrate()
{
    if(m_decoder) {
        AudioEngine::updateBitrate(m_decoder->bitrate());
    }
}

void AudioPlaybackEngine::onBufferProcessed(const AudioBuffer& buffer)
{
    m_totalBufferTime -= buffer.duration();
    emit bufferPlayed(buffer);
}

void AudioPlaybackEngine::onRendererFinished()
{
    if(m_pauseNextTrack) {
        m_pauseNextTrack = false;
        if(playbackState() == PlaybackState::FadingOut) {
            return;
        }
    }

    m_decoder = nullptr;

    if(m_currentTrack.hasCue()) {
        return;
    }

    m_clock.setPaused(true);
    m_clock.sync(m_duration);

    updateTrackStatus(TrackStatus::End);
}

bool AudioPlaybackEngine::trackIsValid() const
{
    const auto status = trackStatus();
    return status != TrackStatus::NoTrack && status != TrackStatus::Invalid && status != TrackStatus::Unreadable;
}

bool AudioPlaybackEngine::trackCanPlay() const
{
    const auto status = trackStatus();
    return status == TrackStatus::Loaded || status == TrackStatus::Buffered || status == TrackStatus::End;
}

bool AudioPlaybackEngine::isFading() const
{
    return playbackState() == PlaybackState::FadingOut;
}

int AudioPlaybackEngine::calculateFadeLength(int initialValue) const
{
    if(initialValue <= 0) {
        return 0;
    }

    if(m_duration == 0) {
        return 0;
    }

    if(m_duration == std::numeric_limits<uint64_t>::max()) {
        return initialValue;
    }

    const auto remaining
        = (m_lastPosition > m_duration) ? m_duration - (m_lastPosition % m_duration) : m_duration - m_lastPosition;
    if(remaining <= 100) {
        return 0;
    }

    if(std::cmp_less(remaining, initialValue)) {
        return static_cast<int>(remaining);
    }

    return initialValue;
}
} // namespace Fooyin

#include "moc_audioplaybackengine.cpp"
