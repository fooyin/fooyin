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

#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>
#include <core/engine/audiodecoder.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <QBasicTimer>
#include <QTimer>
#include <QTimerEvent>

using namespace std::chrono_literals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto BufferInterval = 5ms;
#else
constexpr auto BufferInterval = 5;
#endif

namespace Fooyin {
struct AudioPlaybackEngine::Private
{
    AudioEngine* self;

    SettingsManager* settings;

    AudioClock clock;
    QTimer* positionUpdateTimer{nullptr};

    TrackStatus status{TrackStatus::NoTrack};
    PlaybackState state{PlaybackState::Stopped};
    AudioOutput::State outputState{AudioOutput::State::None};
    uint64_t lastPosition{0};

    uint64_t totalBufferTime{0};
    uint64_t bufferLength{0};

    uint64_t duration{0};
    double volume{1.0};

    AudioFormat format;

    std::unique_ptr<AudioDecoder> decoder;
    AudioRenderer* renderer;

    QBasicTimer bufferTimer;

    explicit Private(AudioEngine* self_, SettingsManager* settings_)
        : self{self_}
        , settings{settings_}
        , bufferLength{static_cast<uint64_t>(settings->value<Settings::Core::BufferLength>())}
        , decoder{std::make_unique<FFmpegDecoder>()}
        , renderer{new AudioRenderer(self)}
    {
        settings->subscribe<Settings::Core::BufferLength>(self, [this](int length) { bufferLength = length; });

        QObject::connect(renderer, &AudioRenderer::bufferProcessed, self,
                         [this](const AudioBuffer& buffer) { totalBufferTime -= buffer.duration(); });
        QObject::connect(renderer, &AudioRenderer::finished, self, [this]() { onRendererFinished(); });
        QObject::connect(renderer, &AudioRenderer::outputStateChanged, self,
                         [this](AudioOutput::State outState) { handleOutputState(outState); });
    }

    QTimer* positionTimer()
    {
        if(!positionUpdateTimer) {
            positionUpdateTimer = new QTimer(self);
            positionUpdateTimer->setInterval(50ms);
            positionUpdateTimer->setTimerType(Qt::PreciseTimer);
            QObject::connect(positionUpdateTimer, &QTimer::timeout, self, [this]() { updatePosition(); });
        }
        return positionUpdateTimer;
    }

    void readNextBuffer()
    {
        if(totalBufferTime >= bufferLength) {
            return;
        }

        const auto bytesLeft = static_cast<size_t>(format.bytesForDuration(bufferLength - totalBufferTime));
        const auto buffer    = decoder->readBuffer(bytesLeft);
        if(buffer.isValid()) {
            totalBufferTime += buffer.duration();
            renderer->queueBuffer(buffer);
        }
        else {
            bufferTimer.stop();
            renderer->queueBuffer({});
            QMetaObject::invokeMethod(self, &AudioEngine::trackAboutToFinish);
        }
    }

    void handleOutputState(AudioOutput::State outState)
    {
        outputState = outState;
        if(outputState == AudioOutput::State::Disconnected) {
            self->pause();
        }
    }

    bool setState(PlaybackState newState)
    {
        auto prevState = std::exchange(state, newState);
        if(prevState != state) {
            emit self->stateChanged(state);
        }

        clock.setPaused(state != PlaybackState::Playing);

        if(state == PlaybackState::Stopped) {
            stopWorkers(true);
        }
        else if(state == PlaybackState::Paused) {
            pauseOutput(true);
        }
        else if(state == PlaybackState::Playing) {
            if(outputState == AudioOutput::State::Disconnected) {
                if(renderer->init(format)) {
                    outputState = AudioOutput::State::None;
                }
                else {
                    state = PlaybackState::Error;
                    emit self->stateChanged(state);
                    return false;
                }
            }

            startPlayback();

            if(prevState == PlaybackState::Paused) {
                pauseOutput(false);
            }
        }

        return true;
    }

    TrackStatus changeTrackStatus(TrackStatus newStatus)
    {
        auto prevStatus = std::exchange(status, newStatus);
        if(prevStatus != status) {
            emit self->trackStatusChanged(status);
        }
        return prevStatus;
    }

    void startBufferTimer()
    {
        bufferTimer.start(BufferInterval, self);
    }

    void updatePosition()
    {
        if(std::exchange(lastPosition, clock.currentPosition()) != lastPosition) {
            emit self->positionChanged(lastPosition);
        }
    }

    bool updateFormat(const AudioFormat& nextFormat)
    {
        const auto prevFormat = std::exchange(format, nextFormat);

        if(settings->value<Settings::Core::GaplessPlayback>() && prevFormat == format
           && state != PlaybackState::Paused) {
            return true;
        }

        if(!renderer->init(format)) {
            format = {};
            return false;
        }

        return true;
    }

    void startPlayback()
    {
        decoder->start();
        startBufferTimer();
        renderer->start();
    }

    void onRendererFinished()
    {
        clock.setPaused(true);
        clock.sync(duration);

        changeTrackStatus(TrackStatus::EndOfTrack);
    }

    void pauseOutput(bool pause)
    {
        if(pause) {
            bufferTimer.stop();
        }
        else {
            startBufferTimer();
        }

        renderer->pause(pause);
    }

    void resetWorkers()
    {
        bufferTimer.stop();
        clock.setPaused(true);
        renderer->reset();
        totalBufferTime = 0;
    }

    void stopWorkers(bool full = false)
    {
        bufferTimer.stop();
        clock.setPaused(true);
        clock.sync();
        renderer->stop();
        if(full) {
            renderer->closeOutput();
            outputState = AudioOutput::State::Disconnected;
        }
        decoder->stop();
        totalBufferTime = 0;
    }
};

AudioPlaybackEngine::AudioPlaybackEngine(SettingsManager* settings, QObject* parent)
    : AudioEngine{parent}
    , p{std::make_unique<Private>(this, settings)}
{ }

AudioPlaybackEngine::~AudioPlaybackEngine()
{
    p->stopWorkers();

    if(p->positionUpdateTimer) {
        p->positionUpdateTimer->deleteLater();
    }
}

void AudioPlaybackEngine::seek(uint64_t pos)
{
    if(!p->decoder->isSeekable()) {
        return;
    }

    p->resetWorkers();

    p->decoder->seek(pos);
    p->clock.sync(pos);

    if(p->state == PlaybackState::Playing) {
        p->clock.setPaused(false);
        p->startBufferTimer();
        p->renderer->start();
    }
    else {
        p->updatePosition();
    }
}

void AudioPlaybackEngine::changeTrack(const Track& track)
{
    p->stopWorkers();

    emit positionChanged(0);

    p->clock.setPaused(true);
    p->clock.sync();

    if(!track.isValid()) {
        p->changeTrackStatus(TrackStatus::InvalidTrack);
        return;
    }

    p->changeTrackStatus(TrackStatus::LoadingTrack);

    if(!p->decoder->init(track.filepath())) {
        p->changeTrackStatus(TrackStatus::InvalidTrack);
        return;
    }

    if(!p->updateFormat(p->decoder->format())) {
        p->setState(PlaybackState::Error);
        p->changeTrackStatus(TrackStatus::NoTrack);
        return;
    }

    p->changeTrackStatus(TrackStatus::LoadedTrack);

    if(p->state == PlaybackState::Playing) {
        p->setState(PlaybackState::Playing);
    }
}

void AudioPlaybackEngine::play()
{
    if(p->status == TrackStatus::NoTrack || p->status == TrackStatus::InvalidTrack) {
        return;
    }

    if(p->status == TrackStatus::EndOfTrack && p->state == PlaybackState::Stopped) {
        seek(0);
        emit positionChanged(0);
    }

    if(p->setState(PlaybackState::Playing)) {
        p->changeTrackStatus(TrackStatus::BufferedTrack);
        p->positionTimer()->start();
    }
}

void AudioPlaybackEngine::pause()
{
    if(p->status == TrackStatus::NoTrack || p->status == TrackStatus::InvalidTrack) {
        return;
    }

    if(p->status == TrackStatus::EndOfTrack && p->state == PlaybackState::Stopped) {
        seek(0);
        emit positionChanged(0);
    }

    if(p->setState(PlaybackState::Paused)) {
        p->positionTimer()->stop();
        p->changeTrackStatus(TrackStatus::BufferedTrack);
    }
}

void AudioPlaybackEngine::stop()
{
    if(p->setState(PlaybackState::Stopped)) {
        p->positionTimer()->stop();
        p->lastPosition = 0;
        emit positionChanged(0);
    }
}

void AudioPlaybackEngine::setVolume(double volume)
{
    p->volume = volume;
    p->renderer->updateVolume(volume);
}

void AudioPlaybackEngine::setAudioOutput(const OutputCreator& output, const QString& device)
{
    const bool playing = (p->state == PlaybackState::Playing || p->state == PlaybackState::Paused);

    p->clock.setPaused(playing);
    if(!p->renderer->isPaused()) {
        p->renderer->pause(playing);
    }

    if(playing) {
        p->bufferTimer.stop();
    }

    p->renderer->updateOutput(output, device);

    if(playing) {
        if(!p->renderer->init(p->format)) {
            p->changeTrackStatus(TrackStatus::NoTrack);
            return;
        }

        p->clock.setPaused(false);
        p->startPlayback();
    }
}

void AudioPlaybackEngine::setOutputDevice(const QString& device)
{
    if(device.isEmpty()) {
        return;
    }

    const bool playing = p->state == PlaybackState::Playing || p->state == PlaybackState::Paused;

    p->clock.setPaused(playing);
    if(!p->renderer->isPaused()) {
        p->renderer->pause(playing);
    }

    if(playing) {
        p->bufferTimer.stop();
    }

    p->renderer->updateDevice(device);

    if(playing) {
        if(!p->renderer->init(p->format)) {
            p->changeTrackStatus(TrackStatus::NoTrack);
            return;
        }

        p->clock.setPaused(false);
        p->startPlayback();
    }
}

void AudioPlaybackEngine::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->bufferTimer.timerId()) {
        p->readNextBuffer();
    }

    QObject::timerEvent(event);
}
} // namespace Fooyin

#include "moc_audioplaybackengine.cpp"
