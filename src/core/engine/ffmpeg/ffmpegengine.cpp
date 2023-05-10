/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "core/engine/audiooutput.h"
#include "ffmpegclock.h"
#include "ffmpegcodec.h"
#include "ffmpegdecoder.h"
#include "ffmpegrenderer.h"
#include "ffmpegstream.h"

extern "C"
{
#include <libavformat/avformat.h>
}

#include <QDebug>
#include <QTime>

namespace Fy::Core::Engine::FFmpeg {
struct FormatContextDeleter
{
    void operator()(AVFormatContext* context) const
    {
        if(context) {
            avformat_close_input(&context);
            avformat_free_context(context);
        }
    }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

struct FFmpegEngine::Private
{
    FFmpegEngine* engine;

    AudioClock clock;
    QTimer positionUpdateTimer;

    AudioOutput* audioOutput;

    uint64_t duration;
    FormatContextPtr context;
    Stream stream;
    bool isSeekable;

    PlaybackState state;

    std::unique_ptr<Decoder> decoder;
    std::unique_ptr<Renderer> renderer;

    std::optional<Codec> codec;

    Private(FFmpegEngine* engine)
        : engine{engine}
        , audioOutput{nullptr}
        , duration{0}
        , isSeekable{false}
        , state{StoppedState}
        , decoder{nullptr}
        , renderer{nullptr}
    {
        positionUpdateTimer.setInterval(50);
        positionUpdateTimer.setTimerType(Qt::PreciseTimer);

        QObject::connect(&positionUpdateTimer, &QTimer::timeout, engine, [this]() {
            updatePosition();
        });
    }

    bool createAVFormatContext(const QString& track)
    {
        AVFormatContext* avContext{nullptr};

        int ret = avformat_open_input(&avContext, track.toUtf8().constData(), nullptr, nullptr);
        if(ret < 0) {
            if(ret == AVERROR(EACCES)) {
                qWarning() << "Invalid format: " << track;
            }
            else if(ret == AVERROR(EINVAL)) {
                qWarning() << "Access denied: " << track;
            }
            return false;
        }

        ret = avformat_find_stream_info(avContext, nullptr);
        if(ret < 0) {
            avformat_close_input(&avContext);
            return false;
        }

        //        av_dump_format(avContext, 0, data, 0);

        isSeekable = !(avContext->ctx_flags & AVFMTCTX_UNSEEKABLE);
        context.reset(avContext);

        updateStream();

        return true;
    }

    void updateStream()
    {
        if(!context) {
            return;
        }
        for(unsigned int i = 0; i < context->nb_streams; ++i) {
            AVStream* avStream = context->streams[i];
            const auto type    = avStream->codecpar->codec_type;

            if(type != AVMEDIA_TYPE_AUDIO) {
                continue;
            }

            duration = avStream->duration;
            stream   = {avStream};
        }
    }

    void runPlayback()
    {
        engine->setState(PlayingState);
        positionUpdateTimer.start();
        engine->stateChanged(PlayingState);
        engine->trackStatusChanged(BufferedTrack);
    }

    void updatePosition()
    {
        if(!renderer) {
            return;
        }
        const uint64_t pos = engine->currentPosition();
        engine->positionChanged(pos);
    }

    void createWorkers()
    {
        clock.setPaused(true);

        if(!codec) {
            codec = Codec::create(stream.avStream());
            if(!codec) {
                qWarning() << "Cannot create codec";
                return;
            }
        }

        decoder  = std::make_unique<Decoder>(context.get(), *codec, stream.index(), &clock);
        renderer = std::make_unique<Renderer>(&clock, audioOutput);

        QObject::connect(decoder.get(), &Decoder::requestHandleFrame, renderer.get(), &Renderer::render);
        QObject::connect(renderer.get(), &Renderer::atEnd, engine, [this]() {
            onRendererFinished();
        });
        QObject::connect(renderer.get(), &Renderer::frameProcessed, decoder.get(), &Decoder::onFrameProcessed);
    }

    void forceUpdate()
    {
        createWorkers();
        updateWorkerPausedState();
    }

    void updateWorkerPausedState()
    {
        const bool paused = state != PlayingState;
        clock.setPaused(paused);

        if(decoder) {
            decoder->setPaused(paused);
        }

        if(renderer) {
            renderer->setPaused(paused);
        }
    }

    void onRendererFinished()
    {
        if(!renderer->isAtEnd()) {
            return;
        }

        if(std::exchange(state, StoppedState) == StoppedState) {
            return;
        }

        clock.setPaused(true);
        clock.sync(duration);

        engine->trackStatusChanged(EndOfTrack);
        engine->trackFinished();
    }
};

FFmpegEngine::FFmpegEngine(AudioPlayer* player)
    : QObject{player}
    , AudioEngine{player}
    , p{std::make_unique<Private>(this)}
{ }

FFmpegEngine::~FFmpegEngine()
{
    disconnect();
}

void FFmpegEngine::seek(uint64_t pos)
{
    if(!p->isSeekable) {
        return;
    }
    uint64_t timestamp = av_rescale_q(pos, {1, 1000}, p->stream.avStream()->time_base);
    p->decoder->seek(timestamp);
    p->renderer->seek(timestamp);

    p->clock.setPaused(true);
    p->clock.sync(pos);

    p->updatePosition();
    if(state() == StoppedState) {
        trackStatusChanged(LoadedTrack);
    }
}

uint64_t FFmpegEngine::currentPosition() const
{
    return p->clock.currentPosition();
}

void FFmpegEngine::changeTrack(const QString& trackPath)
{
    positionChanged(0);

    if(trackPath.isEmpty()) {
        trackStatusChanged(NoTrack);
        return;
    }

    trackStatusChanged(LoadingTrack);

    p->context.reset();
    p->codec = {};

    if(!p->createAVFormatContext(trackPath)) {
        return;
    }

    p->createWorkers();

    trackStatusChanged(LoadedTrack);
}

void FFmpegEngine::setState(PlaybackState state)
{
    if(!p->context) {
        return;
    }

    p->state = state;

    if(state == StoppedState) {
        p->clock.setPaused(true);
        p->clock.sync();
    }

    if(state == PlayingState) {
        p->renderer->updateOutput(p->codec->context());
        p->audioOutput->start();
    }

    p->updateWorkerPausedState();
}

void FFmpegEngine::play()
{
    if(trackStatus() == EndOfTrack && state() == StoppedState) {
        seek(0);
        positionChanged(0);
    }
    p->runPlayback();
}

void FFmpegEngine::pause()
{
    if(trackStatus() == EndOfTrack && state() == StoppedState) {
        seek(0);
        positionChanged(0);
    }
    setState(PausedState);
    p->positionUpdateTimer.stop();
    stateChanged(PausedState);
    trackStatusChanged(BufferedTrack);
}

void FFmpegEngine::stop()
{
    setState(StoppedState);
    p->positionUpdateTimer.stop();
    positionChanged(0);
    stateChanged(StoppedState);
    trackStatusChanged(NoTrack);
}

void FFmpegEngine::setAudioOutput(AudioOutput* output)
{
    if(p->audioOutput == output) {
        return;
    }

    if(std::exchange(p->audioOutput, output) != output) {
        if(p->context) {
            p->forceUpdate();
        }
    }
}
} // namespace Fy::Core::Engine::FFmpeg
