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

#include "ffmpegclock.h"
#include "ffmpegcodec.h"
#include "ffmpegdecoder.h"
#include "ffmpegrenderer.h"
#include "ffmpegstream.h"

#include <core/engine/audiooutput.h>
#include <core/track.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include <QDebug>
#include <QThread>
#include <QTime>
#include <QTimer>

namespace Fy::Core::Engine::FFmpeg {
constexpr int AudioBufferSize = 9;

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
    QTimer* positionUpdateTimer;
    uint64_t lastPos{0};

    uint64_t duration{0};
    FormatContextPtr context;
    Stream stream;
    bool isSeekable{false};

    PlaybackState state{StoppedState};

    AudioOutput* audioOutput;

    QThread* decoderThread;
    QThread* rendererThread;
    Decoder decoder;
    Renderer renderer;

    std::optional<Codec> codec;

    explicit Private(FFmpegEngine* engine)
        : engine{engine}
        , positionUpdateTimer{nullptr}
        , audioOutput{nullptr}
        , decoderThread{new QThread(engine)}
        , rendererThread{new QThread(engine)}
        , renderer{&clock}
    {
        decoder.moveToThread(decoderThread);
        renderer.moveToThread(rendererThread);
    }

    QTimer* positionTimer()
    {
        if(!positionUpdateTimer) {
            positionUpdateTimer = new QTimer(engine);
            positionUpdateTimer->setInterval(50);
            positionUpdateTimer->setTimerType(Qt::PreciseTimer);
            QObject::connect(positionUpdateTimer, &QTimer::timeout, engine, [this]() {
                updatePosition();
            });
        }
        return positionUpdateTimer;
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

    void createCodec(AVStream* avStream)
    {
        if(!avStream) {
            return;
        }

        const AVCodec* avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
        if(!avCodec) {
            return;
        }

        CodecContextPtr avCodecContext(avcodec_alloc_context3(avCodec));
        if(!avCodecContext) {
            return;
        }

        if(avCodecContext->codec_type != AVMEDIA_TYPE_AUDIO) {
            return;
        }

        if(avcodec_parameters_to_context(avCodecContext.get(), avStream->codecpar) < 0) {
            return;
        }

        avCodecContext.get()->pkt_timebase = avStream->time_base;

        AVDictionary* opts{nullptr};
        av_dict_set(&opts, "refcounted_frames", "1", 0);
        av_dict_set(&opts, "threads", "auto", 0);

        if(avcodec_open2(avCodecContext.get(), avCodec, &opts) < 0) {
            return;
        }

        codec = Codec{std::move(avCodecContext), avStream};
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

            stream   = Stream{avStream};
            duration = stream.duration();
        }
    }

    void updatePosition()
    {
        const uint64_t pos = engine->currentPosition();
        if(std::exchange(lastPos, pos) != pos) {
            emit engine->positionChanged(pos);
        }
    }

    void createWorkers()
    {
        if(!codec) {
            createCodec(stream.avStream());
            if(!codec) {
                qWarning() << "Cannot create codec";
                return;
            }
        }

        decoderThread->start();
        rendererThread->start();

        QObject::connect(decoderThread, &QThread::finished, &decoder, &EngineWorker::kill);
        QObject::connect(engine, &FFmpegEngine::resetWorkers, &decoder, &EngineWorker::reset);
        QObject::connect(engine, &FFmpegEngine::killWorkers, &decoder, &EngineWorker::kill);
        QObject::connect(engine, &FFmpegEngine::startDecoder, &decoder, &Decoder::run);

        QObject::connect(rendererThread, &QThread::finished, &renderer, &EngineWorker::kill);
        QObject::connect(engine, &FFmpegEngine::resetWorkers, &renderer, &EngineWorker::reset);
        QObject::connect(engine, &FFmpegEngine::killWorkers, &renderer, &EngineWorker::kill);
        QObject::connect(engine, &FFmpegEngine::startRenderer, &renderer, &Renderer::run);
        QObject::connect(engine, &FFmpegEngine::pauseOutput, &renderer, &Renderer::pauseOutput);
        QObject::connect(engine, &FFmpegEngine::updateOutput, &renderer, &Renderer::updateOutput);
        QObject::connect(engine, &FFmpegEngine::updateDevice, &renderer, &Renderer::updateDevice);

        QObject::connect(&decoder, &Decoder::requestHandleFrame, &renderer, &Renderer::render);
        QObject::connect(&renderer, &Renderer::frameProcessed, &decoder, &Decoder::onFrameProcessed);

        QObject::connect(&renderer, &Renderer::atEnd, engine, [this]() {
            onRendererFinished();
        });
    }

    void startPlayback()
    {
        emit engine->startDecoder(context.get(), &codec.value());
        emit engine->startRenderer(&codec.value(), audioOutput);
    }

    void pauseWorkers(bool pause)
    {
        clock.setPaused(pause);
        decoder.setPaused(pause);
        renderer.setPaused(pause);
    }

    void onRendererFinished()
    {
        if(!renderer.isAtEnd()) {
            return;
        }

        if(std::exchange(state, StoppedState) == StoppedState) {
            return;
        }

        pauseWorkers(true);
        clock.sync(duration);

        engine->trackStatusChanged(EndOfTrack);
        emit engine->trackFinished();
    }

    void resetWorkers()
    {
        pauseWorkers(true);
        emit engine->resetWorkers();
    }

    void killWorkers()
    {
        pauseWorkers(true);
        emit engine->killWorkers();
    }
};

FFmpegEngine::FFmpegEngine(QObject* parent)
    : AudioEngine{parent}
    , p{std::make_unique<Private>(this)}
{ }

FFmpegEngine::~FFmpegEngine() = default;

void FFmpegEngine::shutdown()
{
    //    p->killWorkers();

    p->decoderThread->quit();
    p->decoderThread->wait();
    p->rendererThread->quit();
    p->rendererThread->wait();

    if(p->positionUpdateTimer) {
        p->positionUpdateTimer->deleteLater();
    }
}

void FFmpegEngine::seek(uint64_t pos)
{
    if(!p->isSeekable) {
        return;
    }

    p->resetWorkers();

    const uint64_t timestamp = av_rescale_q(pos, {1, 1000}, p->stream.avStream()->time_base);

    const int flags = pos < p->clock.currentPosition() ? AVSEEK_FLAG_BACKWARD : 0;
    if(av_seek_frame(p->context.get(), p->stream.index(), timestamp, flags) < 0) {
        qWarning() << "Could not seek to position: " << pos;
        return;
    }
    avcodec_flush_buffers(p->codec->context());

    p->clock.sync(pos);
    p->startPlayback();
}

uint64_t FFmpegEngine::currentPosition() const
{
    return p->clock.currentPosition();
}

void FFmpegEngine::changeTrack(const Track& track)
{
    p->killWorkers();

    emit positionChanged(0);

    if(!track.isValid()) {
        trackStatusChanged(NoTrack);
        return;
    }

    trackStatusChanged(LoadingTrack);

    p->context.reset();
    p->codec.reset();

    if(!p->createAVFormatContext(track.filepath())) {
        return;
    }

    p->clock.setPaused(true);
    p->clock.sync();

    p->createWorkers();

    trackStatusChanged(LoadedTrack);
}

void FFmpegEngine::setState(PlaybackState state)
{
    if(!p->context) {
        return;
    }

    auto prevState = std::exchange(p->state, state);

    p->pauseWorkers(p->state != PlayingState);

    if(state == StoppedState) {
        p->killWorkers();
    }
    else if(state == PlayingState) {
        if(prevState == PausedState) {
            emit pauseOutput(false);
        }
        p->startPlayback();
    }
    else if(state == PausedState) {
        emit pauseOutput(true);
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
    p->positionTimer()->start();
    stateChanged(PlayingState);
    trackStatusChanged(BufferedTrack);
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
    stateChanged(PausedState);
    trackStatusChanged(BufferedTrack);
}

void FFmpegEngine::stop()
{
    setState(StoppedState);
    p->positionTimer()->stop();
    emit positionChanged(0);
    stateChanged(StoppedState);
    trackStatusChanged(NoTrack);
}

void FFmpegEngine::setVolume(double /*volume*/) { }

void FFmpegEngine::setAudioOutput(AudioOutput* output)
{
    if(std::exchange(p->audioOutput, output) == output) {
        return;
    }

    const bool playing = state() == PlayingState || state() == PausedState;

    if(playing) {
        p->pauseWorkers(true);
    }

    emit updateOutput(p->audioOutput);

    if(playing) {
        p->startPlayback();
    }
}

void FFmpegEngine::setOutputDevice(const QString& device)
{
    if(device.isEmpty()) {
        return;
    }

    const bool playing = state() == PlayingState || state() == PausedState;

    if(playing) {
        p->pauseWorkers(true);
    }

    emit updateDevice(device);

    if(playing) {
        p->startPlayback();
    }
}
} // namespace Fy::Core::Engine::FFmpeg
