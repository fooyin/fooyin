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
#include <core/engine/audiooutput.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include <QTimer>

using namespace std::chrono_literals;

namespace Fooyin {
struct AudioPlaybackEngine::Private
{
    AudioEngine* self;

    SettingsManager* settings;

    AudioClock clock;
    QTimer* positionUpdateTimer{nullptr};

    TrackStatus status{NoTrack};
    PlaybackState state{StoppedState};
    uint64_t lastPosition{0};

    uint64_t totalBufferTime{0};
    uint64_t bufferLength;

    uint64_t duration{0};
    double volume{1.0};

    AudioFormat format;

    std::unique_ptr<AudioDecoder> decoder;
    AudioRenderer* renderer;

    QTimer* bufferTimer;

    explicit Private(AudioEngine* self_, SettingsManager* settings_)
        : self{self_}
        , settings{settings_}
        , bufferLength{static_cast<uint64_t>(settings->value<Settings::Core::BufferLength>())}
        , decoder{std::make_unique<FFmpegDecoder>()}
        , renderer{new AudioRenderer(self)}
        , bufferTimer{new QTimer(self)}
    {
        bufferTimer->setInterval(5ms);

        settings->subscribe<Settings::Core::BufferLength>(self, [this](int length) { bufferLength = length; });

        QObject::connect(renderer, &AudioRenderer::bufferProcessed, self, [this](const AudioBuffer& buffer) {
            totalBufferTime -= buffer.duration();
            clock.sync(buffer.startTime());
        });
        QObject::connect(renderer, &AudioRenderer::finished, self, [this]() { onRendererFinished(); });

        QObject::connect(bufferTimer, &QTimer::timeout, self, [this]() { readNextBuffer(); });
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

        const auto buffer = decoder->readBuffer();
        if(buffer.isValid()) {
            totalBufferTime += buffer.duration();
            renderer->queueBuffer(buffer);
        }
        else {
            bufferTimer->stop();
            renderer->queueBuffer({});
        }
    }

    PlaybackState changeState(PlaybackState newState)
    {
        auto prevState = std::exchange(state, newState);
        if(prevState != state) {
            QMetaObject::invokeMethod(self, "stateChanged", Q_ARG(PlaybackState, state));
        }
        return prevState;
    }

    TrackStatus changeTrackStatus(TrackStatus newStatus)
    {
        auto prevStatus = std::exchange(status, newStatus);
        if(prevStatus != status) {
            QMetaObject::invokeMethod(self, "trackStatusChanged", Q_ARG(TrackStatus, status));
        }
        return prevStatus;
    }

    void updatePosition()
    {
        if(std::exchange(lastPosition, clock.currentPosition()) != lastPosition) {
            QMetaObject::invokeMethod(self, "positionChanged", Q_ARG(uint64_t, lastPosition));
        }
    }

    bool updateFormat(const AudioFormat& nextFormat)
    {
        const auto prevFormat = std::exchange(format, nextFormat);

        if(settings->value<Settings::Core::GaplessPlayback>() && prevFormat == format) {
            return true;
        }

        if(!renderer->init(format)) {
            format = {};
            return false;
        }

        return true;
    }

    void startPlayback() const
    {
        decoder->start();
        bufferTimer->start();
        renderer->start();
    }

    void onRendererFinished()
    {
        if(changeState(StoppedState) == StoppedState) {
            return;
        }

        clock.setPaused(true);
        clock.sync(duration);

        changeTrackStatus(EndOfTrack);
    }

    void pauseOutput(bool pause) const
    {
        if(pause) {
            bufferTimer->stop();
        }
        else {
            bufferTimer->start();
        }

        renderer->pause(pause);
    }

    void resetWorkers()
    {
        bufferTimer->stop();
        clock.setPaused(true);
        renderer->reset();
        totalBufferTime = 0;
    }

    void stopWorkers()
    {
        bufferTimer->stop();
        clock.setPaused(true);
        renderer->stop();
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

PlaybackState AudioPlaybackEngine::state() const
{
    return p->state;
}

TrackStatus AudioPlaybackEngine::trackStatus() const
{
    return p->status;
}

uint64_t AudioPlaybackEngine::position() const
{
    return p->lastPosition;
}

void AudioPlaybackEngine::seek(uint64_t pos)
{
    if(!p->decoder->isSeekable()) {
        return;
    }

    p->resetWorkers();

    p->decoder->seek(pos);
    p->clock.sync(pos);

    if(state() == PlayingState) {
        p->clock.setPaused(false);
        p->bufferTimer->start();
        p->renderer->start();
    }
}

void AudioPlaybackEngine::changeTrack(const Track& track)
{
    p->stopWorkers();

    emit positionChanged(0);

    p->clock.setPaused(true);
    p->clock.sync();

    if(!track.isValid()) {
        p->changeTrackStatus(InvalidTrack);
        return;
    }

    p->changeTrackStatus(LoadingTrack);

    if(!p->decoder->init(track.filepath())) {
        p->changeTrackStatus(InvalidTrack);
        return;
    }

    if(!p->updateFormat(p->decoder->format())) {
        p->changeTrackStatus(NoTrack);
        return;
    }

    p->changeTrackStatus(LoadedTrack);
}

void AudioPlaybackEngine::setState(PlaybackState state)
{
    const auto prevState = p->changeState(state);

    p->clock.setPaused(state != PlayingState);

    if(state == StoppedState) {
        p->stopWorkers();
    }
    else if(state == PlayingState) {
        if(prevState == PausedState) {
            p->pauseOutput(false);
        }
        p->startPlayback();
    }
    else if(state == PausedState) {
        p->pauseOutput(true);
    }
}

void AudioPlaybackEngine::play()
{
    if(trackStatus() == NoTrack || trackStatus() == InvalidTrack) {
        return;
    }

    if(trackStatus() == EndOfTrack && state() == StoppedState) {
        seek(0);
        emit positionChanged(0);
    }

    setState(PlayingState);
    p->changeTrackStatus(BufferedTrack);
    p->positionTimer()->start();
}

void AudioPlaybackEngine::pause()
{
    if(trackStatus() == NoTrack || trackStatus() == InvalidTrack) {
        return;
    }

    if(trackStatus() == EndOfTrack && state() == StoppedState) {
        seek(0);
        emit positionChanged(0);
    }

    setState(PausedState);
    p->positionTimer()->stop();
    p->changeTrackStatus(BufferedTrack);
}

void AudioPlaybackEngine::stop()
{
    setState(StoppedState);
    p->positionTimer()->stop();
    p->changeTrackStatus(NoTrack);
    emit positionChanged(0);
}

void AudioPlaybackEngine::setVolume(double volume)
{
    p->volume = volume;
    p->renderer->updateVolume(volume);
}

void AudioPlaybackEngine::setAudioOutput(const OutputCreator& output)
{
    const bool playing = (state() == PlayingState || state() == PausedState);

    p->clock.setPaused(playing);
    p->renderer->pause(playing);

    if(playing) {
        p->bufferTimer->stop();
    }

    p->renderer->updateOutput(output);

    if(playing) {
        if(!p->renderer->init(p->format)) {
            p->changeTrackStatus(NoTrack);
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

    const bool playing = state() == PlayingState || state() == PausedState;

    p->clock.setPaused(playing);
    p->renderer->pause(playing);

    if(playing) {
        p->bufferTimer->stop();
    }

    p->renderer->updateDevice(device);

    if(playing) {
        if(!p->renderer->init(p->format)) {
            p->changeTrackStatus(NoTrack);
            return;
        }
        p->clock.setPaused(false);
        p->startPlayback();
    }
}
} // namespace Fooyin

#include "moc_audioplaybackengine.cpp"
