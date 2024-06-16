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
#include "engine/ffmpeg/ffmpegdecoder.h"
#include "internalcoresettings.h"

#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>
#include <core/engine/audiodecoder.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <QBasicTimer>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>

using namespace std::chrono_literals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto BufferInterval = 5ms;
#else
constexpr auto BufferInterval = 5;
#endif

constexpr auto MaxDecodeLength = 1000;

namespace Fooyin {
struct AudioPlaybackEngine::Private
{
    AudioPlaybackEngine* m_self;

    SettingsManager* m_settings;

    AudioClock m_clock;
    QTimer* m_positionUpdateTimer{nullptr};

    TrackStatus m_status{TrackStatus::NoTrack};
    PlaybackState m_state{PlaybackState::Stopped};
    AudioOutput::State m_outputState{AudioOutput::State::None};

    uint64_t m_startPosition{0};
    uint64_t m_endPosition{0};
    uint64_t m_lastPosition{0};

    uint64_t m_totalBufferTime{0};
    uint64_t m_bufferLength{0};

    uint64_t m_duration{0};
    double m_volume{1.0};
    bool m_ending{false};
    bool m_pausing{false};

    Track m_currentTrack;
    AudioFormat m_format;

    std::unique_ptr<AudioDecoder> m_decoder;
    AudioRenderer* m_renderer;

    QBasicTimer m_bufferTimer;
    QBasicTimer m_pauseTimer;

    FadingIntervals m_fadeIntervals;

    explicit Private(AudioPlaybackEngine* self, SettingsManager* settings)
        : m_self{self}
        , m_settings{settings}
        , m_bufferLength{static_cast<uint64_t>(m_settings->value<Settings::Core::BufferLength>())}
        , m_decoder{std::make_unique<FFmpegDecoder>()}
        , m_renderer{new AudioRenderer(m_self)}
        , m_fadeIntervals{m_settings->value<Settings::Core::Internal::FadingIntervals>().value<FadingIntervals>()}
    {
        m_settings->subscribe<Settings::Core::BufferLength>(m_self, [this](int length) { m_bufferLength = length; });
        m_settings->subscribe<Settings::Core::Internal::FadingIntervals>(
            m_self, [this](const QVariant& fading) { m_fadeIntervals = fading.value<FadingIntervals>(); });

        QObject::connect(m_renderer, &AudioRenderer::bufferProcessed, m_self,
                         [this](const AudioBuffer& buffer) { m_totalBufferTime -= buffer.duration(); });
        QObject::connect(m_renderer, &AudioRenderer::finished, m_self, [this]() { onRendererFinished(); });
        QObject::connect(m_renderer, &AudioRenderer::outputStateChanged, m_self,
                         [this](AudioOutput::State outState) { handleOutputState(outState); });
    }

    QTimer* positionTimer()
    {
        if(!m_positionUpdateTimer) {
            m_positionUpdateTimer = new QTimer(m_self);
            m_positionUpdateTimer->setInterval(50ms);
            m_positionUpdateTimer->setTimerType(Qt::PreciseTimer);
            QObject::connect(m_positionUpdateTimer, &QTimer::timeout, m_self, [this]() { updatePosition(); });
        }
        return m_positionUpdateTimer;
    }

    void readNextBuffer()
    {
        if(m_totalBufferTime >= m_bufferLength) {
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

        if(!buffer.isValid() || buffer.endTime() >= m_endPosition) {
            m_bufferTimer.stop();
            m_renderer->queueBuffer({});
            m_ending = true;
            QMetaObject::invokeMethod(m_self, &AudioEngine::trackAboutToFinish);
        }
    }

    void handleOutputState(AudioOutput::State outState)
    {
        m_outputState = outState;
        if(m_outputState == AudioOutput::State::Disconnected) {
            m_self->pause();
        }
    }

    void updateState(PlaybackState newState)
    {
        if(std::exchange(m_state, newState) != m_state) {
            emit m_self->stateChanged(newState);
        }
        m_clock.setPaused(newState != PlaybackState::Playing);
    }

    void stop()
    {
        auto delayedStop = [this]() {
            stopWorkers(true);
        };

        const bool canFade
            = m_settings->value<Settings::Core::Internal::EngineFading>() && m_fadeIntervals.outPauseStop > 0;
        if(canFade) {
            QObject::connect(m_renderer, &AudioRenderer::paused, m_self, delayedStop, Qt::SingleShotConnection);
            m_renderer->pause(true, m_fadeIntervals.outPauseStop);
        }
        else {
            stopWorkers(true);
        }

        updateState(PlaybackState::Stopped);
        positionTimer()->stop();
        m_lastPosition = 0;
        emit m_self->positionChanged(0);
    }

    void pause()
    {
        auto delayedPause = [this]() {
            pauseOutput(true);
            updateState(PlaybackState::Paused);
            m_clock.setPaused(true);
        };

        QObject::connect(m_renderer, &AudioRenderer::paused, m_self, delayedPause, Qt::SingleShotConnection);

        const int fadeInterval
            = m_settings->value<Settings::Core::Internal::EngineFading>() ? m_fadeIntervals.outPauseStop : 0;
        m_renderer->pause(true, fadeInterval);
    }

    void play()
    {
        if(m_outputState == AudioOutput::State::Disconnected) {
            if(m_renderer->init(m_format)) {
                m_outputState = AudioOutput::State::None;
            }
            else {
                updateState(PlaybackState::Error);
                return;
            }
        }

        const auto prevState = m_state;

        startPlayback();
        if(m_state == PlaybackState::Stopped && m_currentTrack.offset() > 0) {
            m_decoder->seek(m_currentTrack.offset());
        }

        updateState(PlaybackState::Playing);

        const bool canFade = m_settings->value<Settings::Core::Internal::EngineFading>()
                          && (prevState == PlaybackState::Paused || m_renderer->isFading());
        const int fadeInterval = canFade ? m_fadeIntervals.inPauseStop : 0;

        m_renderer->pause(false, fadeInterval);
        changeTrackStatus(TrackStatus::BufferedTrack);
        positionTimer()->start();
    }

    void enterErrorState()
    {
        updateState(PlaybackState::Error);
    }

    TrackStatus changeTrackStatus(TrackStatus newStatus)
    {
        auto prevStatus = std::exchange(m_status, newStatus);
        if(prevStatus != m_status) {
            emit m_self->trackStatusChanged(m_status);
        }
        return prevStatus;
    }

    void startBufferTimer()
    {
        m_bufferTimer.start(BufferInterval, m_self);
    }

    void updatePosition()
    {
        const auto currentPosition = m_startPosition + m_clock.currentPosition();
        if(std::exchange(m_lastPosition, currentPosition) != m_lastPosition) {
            emit m_self->positionChanged(m_lastPosition - m_startPosition);
        }

        if(m_currentTrack.hasCue() && m_lastPosition >= m_endPosition) {
            m_clock.setPaused(true);
            m_clock.sync(m_duration);
            changeTrackStatus(TrackStatus::EndOfTrack);
        }
    }

    bool updateFormat(const AudioFormat& nextFormat)
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

    void startPlayback()
    {
        m_decoder->start();
        startBufferTimer();
        m_renderer->start();
    }

    void onRendererFinished()
    {
        if(m_currentTrack.hasCue()) {
            return;
        }

        m_clock.setPaused(true);
        m_clock.sync(m_duration);

        changeTrackStatus(TrackStatus::EndOfTrack);
    }

    void pauseOutput(bool pause)
    {
        if(pause) {
            m_bufferTimer.stop();
        }
        else {
            startBufferTimer();
        }
    }

    void resetWorkers()
    {
        m_bufferTimer.stop();
        m_clock.setPaused(true);
        m_renderer->reset();
        m_totalBufferTime = 0;
    }

    void stopWorkers(bool full = false)
    {
        m_bufferTimer.stop();
        m_clock.setPaused(true);
        m_clock.sync();
        m_renderer->stop();
        if(full) {
            m_renderer->closeOutput();
            m_outputState = AudioOutput::State::Disconnected;
        }
        m_decoder->stop();
        m_totalBufferTime = 0;
    }
};

AudioPlaybackEngine::AudioPlaybackEngine(SettingsManager* settings, QObject* parent)
    : AudioEngine{parent}
    , p{std::make_unique<Private>(this, settings)}
{ }

AudioPlaybackEngine::~AudioPlaybackEngine()
{
    AudioPlaybackEngine::stop();

    p->stopWorkers();

    if(p->m_positionUpdateTimer) {
        p->m_positionUpdateTimer->deleteLater();
    }
}

void AudioPlaybackEngine::seek(uint64_t pos)
{
    if(!p->m_decoder->isSeekable()) {
        return;
    }

    p->resetWorkers();

    p->m_decoder->seek(pos + p->m_startPosition);
    p->m_clock.sync(pos);

    if(p->m_state == PlaybackState::Playing) {
        p->m_clock.setPaused(false);
        p->startBufferTimer();
        p->m_renderer->start();
    }
    else {
        p->updatePosition();
    }
}

void AudioPlaybackEngine::changeTrack(const Track& track)
{
    const Track prevTrack = std::exchange(p->m_currentTrack, track);

    auto setupDuration = [this, &track]() {
        p->m_duration      = track.duration();
        p->m_startPosition = track.offset();
        p->m_endPosition   = p->m_startPosition + p->m_duration;
        p->m_lastPosition  = p->m_startPosition;
    };

    if(p->m_ending && track.filepath() == prevTrack.filepath() && p->m_endPosition == track.offset()) {
        emit positionChanged(0);
        p->m_ending = false;
        p->m_clock.sync(0);
        setupDuration();
        p->changeTrackStatus(TrackStatus::BufferedTrack);
        if(p->m_state == PlaybackState::Playing) {
            p->play();
        }
        return;
    }

    p->stopWorkers();

    emit positionChanged(0);

    p->m_ending = false;
    p->m_clock.setPaused(true);
    p->m_clock.sync();

    if(!track.isValid()) {
        p->changeTrackStatus(TrackStatus::InvalidTrack);
        return;
    }

    p->changeTrackStatus(TrackStatus::LoadingTrack);

    if(!p->m_decoder->init(track.filepath())) {
        p->changeTrackStatus(TrackStatus::InvalidTrack);
        return;
    }

    if(!p->updateFormat(p->m_decoder->format())) {
        p->enterErrorState();
        p->changeTrackStatus(TrackStatus::NoTrack);
        return;
    }

    p->changeTrackStatus(TrackStatus::LoadedTrack);

    setupDuration();

    if(track.offset() > 0) {
        p->m_decoder->seek(track.offset());
    }

    if(p->m_state == PlaybackState::Playing) {
        p->play();
    }
}

void AudioPlaybackEngine::play()
{
    if(p->m_status == TrackStatus::NoTrack || p->m_status == TrackStatus::InvalidTrack) {
        return;
    }

    if(p->m_status == TrackStatus::EndOfTrack && p->m_state == PlaybackState::Stopped) {
        seek(0);
        emit positionChanged(0);
    }

    p->play();
}

void AudioPlaybackEngine::pause()
{
    if(p->m_status == TrackStatus::NoTrack || p->m_status == TrackStatus::InvalidTrack) {
        return;
    }

    if(p->m_status == TrackStatus::EndOfTrack && p->m_state == PlaybackState::Stopped) {
        seek(0);
        emit positionChanged(0);
    }
    else {
        p->pause();
    }
}

void AudioPlaybackEngine::stop()
{
    if(p->m_state != PlaybackState::Stopped) {
        p->stop();
    }
}

void AudioPlaybackEngine::setVolume(double volume)
{
    p->m_volume = volume;
    p->m_renderer->updateVolume(volume);
}

void AudioPlaybackEngine::setAudioOutput(const OutputCreator& output, const QString& device)
{
    const bool playing = p->m_state == PlaybackState::Playing;

    if(playing) {
        p->m_clock.setPaused(true);
        p->m_renderer->pause(true);
    }

    p->m_renderer->updateOutput(output, device);

    if(playing) {
        if(!p->m_renderer->init(p->m_format)) {
            p->changeTrackStatus(TrackStatus::NoTrack);
            return;
        }

        p->m_clock.setPaused(false);
        p->m_renderer->start();
        p->m_renderer->pause(false);
    }
}

void AudioPlaybackEngine::setOutputDevice(const QString& device)
{
    if(device.isEmpty()) {
        return;
    }

    const bool playing = p->m_state == PlaybackState::Playing;

    if(playing) {
        p->m_clock.setPaused(true);
        p->m_renderer->pause(true);
    }

    p->m_renderer->updateDevice(device);

    if(playing) {
        if(!p->m_renderer->init(p->m_format)) {
            p->changeTrackStatus(TrackStatus::NoTrack);
            return;
        }

        p->m_clock.setPaused(false);
        p->startPlayback();
        p->m_renderer->pause(false);
    }
}

void AudioPlaybackEngine::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_bufferTimer.timerId()) {
        p->readNextBuffer();
    }

    QObject::timerEvent(event);
}
} // namespace Fooyin

#include "moc_audioplaybackengine.cpp"
