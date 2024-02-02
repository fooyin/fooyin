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

#include "ffmpegengine.h"

#include "ffmpegclock.h"
#include "ffmpegdecoder.h"
#include "ffmpegrenderer.h"

#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>
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
using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
struct FFmpegEngine::Private
{
    FFmpegEngine* self;

    SettingsManager* settings;

    AudioClock clock;
    QTimer* positionUpdateTimer{nullptr};
    uint64_t lastPos{0};

    uint64_t duration{0};
    double volume{1.0};

    PlaybackState state{StoppedState};

    AudioOutput* audioOutput{nullptr};
    AudioFormat format;

    FFmpegDecoder* decoder;
    FFmpegRenderer* renderer;

    QTimer* bufferTimer;

    explicit Private(FFmpegEngine* self_, SettingsManager* settings)
        : self{self_}
        , settings{settings}
        , decoder{new FFmpegDecoder(self)}
        , renderer{new FFmpegRenderer(self)}
        , bufferTimer{new QTimer(self)}
    {
        QObject::connect(decoder, &FFmpegDecoder::finished, self, [this]() {
            bufferTimer->stop();
            renderer->queueBuffer({});
        });
        QObject::connect(renderer, &FFmpegRenderer::finished, self, [this]() { onRendererFinished(); });

        QObject::connect(bufferTimer, &QTimer::timeout, self, [this]() {
            if(renderer->queuedBuffers() < 15) {
                const auto buffer = decoder->readBuffer();
                if(buffer.isValid()) {
                    renderer->queueBuffer(buffer);
                }
            }
        });
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

    void updatePosition()
    {
        const uint64_t pos = self->currentPosition();
        if(std::exchange(lastPos, pos) != pos) {
            QMetaObject::invokeMethod(self, "positionChanged", Q_ARG(uint64_t, pos));
        }
    }

    void updateFormat(const AudioFormat& nextFormat)
    {
        const auto prevFormat = std::exchange(format, nextFormat);

        if(audioOutput->initialised() && settings->value<Settings::Core::GaplessPlayback>() && prevFormat == format) {
            return;
        }

        renderer->init({.format = format, .volume = volume});
    }

    void startPlayback()
    {
        decoder->start();
        bufferTimer->start();
        renderer->start();
    }

    void onRendererFinished()
    {
        if(std::exchange(state, StoppedState) == StoppedState) {
            return;
        }

        clock.setPaused(true);
        clock.sync(duration);

        self->changeTrackStatus(EndOfTrack);
    }

    void pauseOutput(bool pause)
    {
        if(pause) {
            bufferTimer->stop();
        }
        else {
            bufferTimer->start();
        }

        audioOutput->setPaused(pause);
        renderer->pause(pause);
    }

    void killWorkers()
    {
        bufferTimer->stop();
        clock.setPaused(true);

        decoder->stop();
        renderer->stop();
    }
};

FFmpegEngine::FFmpegEngine(SettingsManager* settings, QObject* parent)
    : AudioEngine{parent}
    , p{std::make_unique<Private>(this, settings)}
{ }

FFmpegEngine::~FFmpegEngine()
{
    p->killWorkers();

    p->audioOutput->uninit();

    if(p->positionUpdateTimer) {
        p->positionUpdateTimer->deleteLater();
    }
}
void FFmpegEngine::seek(uint64_t pos)
{
    if(!p->decoder->isSeekable()) {
        return;
    }

    p->bufferTimer->stop();
    p->clock.setPaused(true);
    p->renderer->stop();
    p->audioOutput->reset();

    p->decoder->seek(pos);
    p->clock.sync(pos);

    p->clock.setPaused(false);
    p->bufferTimer->start();
    p->renderer->start();
}

uint64_t FFmpegEngine::currentPosition() const
{
    return p->clock.currentPosition();
}

void FFmpegEngine::changeTrack(const Track& track)
{
    p->killWorkers();

    emit positionChanged(0);

    p->clock.setPaused(true);
    p->clock.sync();

    if(!track.isValid()) {
        changeTrackStatus(InvalidTrack);
        return;
    }

    changeTrackStatus(LoadingTrack);

    if(!p->decoder->init(track.filepath())) {
        changeTrackStatus(InvalidTrack);
        return;
    }

    p->updateFormat(p->decoder->format());

    changeTrackStatus(LoadedTrack);
}

void FFmpegEngine::setState(PlaybackState state)
{
    changeState(state);

    p->clock.setPaused(state != PlayingState);

    auto prevState = std::exchange(p->state, state);

    if(state == StoppedState) {
        p->killWorkers();
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

void FFmpegEngine::play()
{
    if(!p->audioOutput || trackStatus() == NoTrack) {
        return;
    }

    if(trackStatus() == EndOfTrack && state() == StoppedState) {
        seek(0);
        emit positionChanged(0);
    }

    setState(PlayingState);
    changeTrackStatus(BufferedTrack);
    p->positionTimer()->start();
}

void FFmpegEngine::pause()
{
    if(trackStatus() == NoTrack) {
        return;
    }

    if(trackStatus() == EndOfTrack && state() == StoppedState) {
        seek(0);
        emit positionChanged(0);
    }

    setState(PausedState);
    p->positionTimer()->stop();
    changeTrackStatus(BufferedTrack);
}

void FFmpegEngine::stop()
{
    setState(StoppedState);
    p->positionTimer()->stop();
    changeTrackStatus(NoTrack);
    emit positionChanged(0);
}

void FFmpegEngine::setVolume(double volume)
{
    p->volume = volume;
    p->renderer->updateVolume(volume);
}

void FFmpegEngine::setAudioOutput(AudioOutput* output)
{
    if(std::exchange(p->audioOutput, output) == output) {
        return;
    }

    const bool playing = state() == PlayingState || state() == PausedState;

    p->clock.setPaused(playing);
    p->renderer->pause(playing);

    if(playing) {
        p->bufferTimer->stop();
    }

    p->renderer->updateOutput(p->audioOutput);

    if(playing) {
        p->clock.setPaused(false);
        p->startPlayback();
    }
}

void FFmpegEngine::setOutputDevice(const QString& device)
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
        p->clock.setPaused(false);
        p->startPlayback();
    }
}
} // namespace Fooyin

#include "moc_ffmpegengine.cpp"
