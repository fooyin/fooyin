/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "ffmpegaudiobuffer.h"
#include "ffmpegclock.h"
#include "ffmpegcodec.h"
#include "ffmpegdecoder.h"
#include "ffmpegrenderer.h"
#include "ffmpegstream.h"
#include "ffmpegutils.h"

#include <core/coresettings.h>
#include <core/engine/audiooutput.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include <QDebug>
#include <QTime>
#include <QTimer>

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

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

FormatContextPtr createAVFormatContext(const QString& track)
{
    FormatContextPtr formatContext;
    AVFormatContext* avContext{nullptr};

    const int ret = avformat_open_input(&avContext, track.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        if(ret == AVERROR(EACCES)) {
            qWarning() << "Invalid format: " << track;
        }
        else if(ret == AVERROR(EINVAL)) {
            qWarning() << "Access denied: " << track;
        }
        return nullptr;
    }

    if(avformat_find_stream_info(avContext, nullptr) < 0) {
        avformat_close_input(&avContext);
        Fooyin::Utils::printError(u"Could not find stream info"_s);
        return nullptr;
    }

    //        av_dump_format(avContext, 0, data, 0);

    formatContext.reset(avContext);

    return formatContext;
}

std::optional<Fooyin::Stream> findStream(const FormatContextPtr& formatContext)
{
    for(unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        AVStream* avStream = formatContext->streams[i];
        const auto type    = avStream->codecpar->codec_type;

        if(type == AVMEDIA_TYPE_AUDIO) {
            return Fooyin::Stream{avStream};
            break;
        }
    }

    return {};
}

std::optional<Fooyin::Codec> createCodec(AVStream* avStream)
{
    if(!avStream) {
        return {};
    }

    const AVCodec* avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
    if(!avCodec) {
        Fooyin::Utils::printError(u"Could not find a decoder for stream"_s);
        return {};
    }

    Fooyin::CodecContextPtr avCodecContext{avcodec_alloc_context3(avCodec)};
    if(!avCodecContext) {
        Fooyin::Utils::printError(u"Could not allocate context"_s);
        return {};
    }

    if(avCodecContext->codec_type != AVMEDIA_TYPE_AUDIO) {
        return {};
    }

    if(avcodec_parameters_to_context(avCodecContext.get(), avStream->codecpar) < 0) {
        Fooyin::Utils::printError(u"Could not obtain codec parameters"_s);
        return {};
    }

    avCodecContext.get()->pkt_timebase = avStream->time_base;

    if(avcodec_open2(avCodecContext.get(), avCodec, nullptr) < 0) {
        Fooyin::Utils::printError(u"Could not initialise codec context"_s);
        return {};
    }

    return Fooyin::Codec{std::move(avCodecContext), avStream};
}

namespace Fooyin {
struct FFmpegEngine::Private
{
    FFmpegEngine* engine;

    SettingsManager* settings;

    AudioClock clock;
    QTimer* positionUpdateTimer{nullptr};
    uint64_t lastPos{0};

    uint64_t duration{0};
    FormatContextPtr context;
    Stream stream;
    bool isSeekable{false};
    double volume{1.0};

    PlaybackState state{StoppedState};

    AudioOutput* audioOutput{nullptr};
    std::optional<OutputContext> outputContext;

    Decoder* decoder{nullptr};
    Renderer* renderer{nullptr};

    std::optional<Codec> codec;
    AudioFormat audioFormat;

    explicit Private(FFmpegEngine* engine, SettingsManager* settings)
        : engine{engine}
        , settings{settings}
    { }

    QTimer* positionTimer()
    {
        if(!positionUpdateTimer) {
            positionUpdateTimer = new QTimer(engine);
            positionUpdateTimer->setInterval(50ms);
            positionUpdateTimer->setTimerType(Qt::PreciseTimer);
            QObject::connect(positionUpdateTimer, &QTimer::timeout, engine, [this]() { updatePosition(); });
        }
        return positionUpdateTimer;
    }

    std::optional<OutputContext> updateOutputContext()
    {
        auto prevContext = outputContext;

        if(!audioOutput || !codec || !codec->context()) {
            outputContext = {};
            return prevContext;
        }

        OutputContext updatedContext;

        updatedContext.format        = audioFormat;
        updatedContext.channelLayout = codec->context()->ch_layout;
        updatedContext.volume        = volume;

        outputContext = updatedContext;

        return prevContext;
    }

    void updatePosition()
    {
        const uint64_t pos = engine->currentPosition();
        if(std::exchange(lastPos, pos) != pos) {
            emit engine->positionChanged(pos);
        }
    }

    bool openTrack(const QString& filepath)
    {
        context.reset();
        codec.reset();

        context = createAVFormatContext(filepath);

        if(!context) {
            return false;
        }

        isSeekable = !(context->ctx_flags & AVFMTCTX_UNSEEKABLE);

        auto audioStream = findStream(context);

        if(!audioStream) {
            return false;
        }

        stream      = audioStream.value();
        audioFormat = Utils::audioFormatFromCodec(stream.avStream()->codecpar);

        codec = createCodec(stream.avStream());

        return !!codec;
    }

    void startPlayback()
    {
        if(!decoder || !renderer) {
            return;
        }

        QMetaObject::invokeMethod(
            decoder,
            [this] {
                if(context && codec) {
                    decoder->run(context.get(), &codec.value(), audioFormat);
                }
            },
            Qt::QueuedConnection);

        QMetaObject::invokeMethod(
            renderer,
            [this] {
                if(outputContext) {
                    renderer->run(outputContext.value(), audioOutput);
                }
            },
            Qt::QueuedConnection);
    }

    void pauseWorkers(bool pause)
    {
        clock.setPaused(pause);
        decoder->setPaused(pause);
        renderer->setPaused(pause);
    }

    void onRendererFinished()
    {
        if(std::exchange(state, StoppedState) == StoppedState) {
            return;
        }

        pauseWorkers(true);
        clock.sync(duration);

        engine->changeTrackStatus(EndOfTrack);
    }

    void pauseOutput(bool pause) const
    {
        if(renderer) {
            renderer->pauseOutput(pause);
        }
    }

    void resetWorkers()
    {
        pauseWorkers(true);

        if(decoder) {
            decoder->reset();
        }
        if(renderer) {
            renderer->reset();
        }
    }

    void killWorkers()
    {
        pauseWorkers(true);

        if(decoder) {
            decoder->kill();
        }
        if(renderer) {
            renderer->kill();
        }
    }
};

FFmpegEngine::FFmpegEngine(SettingsManager* settings, QObject* parent)
    : AudioEngine{parent}
    , p{std::make_unique<Private>(this, settings)}
{ }

FFmpegEngine::~FFmpegEngine() = default;

void FFmpegEngine::seek(uint64_t pos)
{
    if(!p->isSeekable) {
        return;
    }

    p->resetWorkers();

    const int64_t timestamp = av_rescale_q(static_cast<int64_t>(pos), {1, 1000}, p->stream.avStream()->time_base);

    const int flags = pos < p->clock.currentPosition() ? AVSEEK_FLAG_BACKWARD : 0;
    if(av_seek_frame(p->context.get(), p->stream.index(), timestamp, flags) < 0) {
        qWarning() << "Could not seek to position: " << pos;
        return;
    }

    if(p->codec) {
        avcodec_flush_buffers(p->codec->context());
    }

    p->clock.sync(pos);
    p->startPlayback();
    p->clock.setPaused(false);
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

    if(!p->openTrack(track.filepath())) {
        changeTrackStatus(InvalidTrack);
        return;
    }

    const auto prevContext = p->updateOutputContext();

    if(p->audioOutput->initialised()) {
        if(p->settings->value<Settings::Core::GaplessPlayback>()) {
            if(!prevContext || (p->outputContext && p->outputContext.value() != prevContext.value())) {
                p->audioOutput->uninit();
            }
        }
        else {
            p->audioOutput->uninit();
        }
    }

    changeTrackStatus(LoadedTrack);
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
    p->positionTimer()->start();
    changeState(PlayingState);
    changeTrackStatus(BufferedTrack);
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
    changeState(PausedState);
    changeTrackStatus(BufferedTrack);
}

void FFmpegEngine::stop()
{
    setState(StoppedState);
    p->positionTimer()->stop();
    emit positionChanged(0);
    changeState(StoppedState);
    changeTrackStatus(NoTrack);
}

void FFmpegEngine::setVolume(double volume)
{
    p->volume = volume;
    if(p->renderer) {
        p->renderer->updateVolume(volume);
    }
}

void FFmpegEngine::setAudioOutput(AudioOutput* output)
{
    if(std::exchange(p->audioOutput, output) == output) {
        return;
    }

    const bool playing = state() == PlayingState || state() == PausedState;

    if(playing) {
        p->pauseWorkers(true);
    }

    if(p->renderer) {
        p->renderer->updateOutput(p->audioOutput);
    }

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

    if(p->renderer) {
        p->renderer->updateDevice(device);
    }

    if(playing) {
        p->startPlayback();
    }
}

void FFmpegEngine::startup()
{
    p->decoder  = new Decoder(this);
    p->renderer = new Renderer(this);

    QObject::connect(p->decoder, &Decoder::audioBufferDecoded, p->renderer, &Renderer::render);
    QObject::connect(p->renderer, &Renderer::audioBufferProcessed, p->decoder, &Decoder::onBufferProcessed);
    QObject::connect(p->renderer, &Renderer::audioBufferProcessed, p->engine, [this](const FFmpegAudioBuffer& buffer) {
        const uint64_t pos = buffer.startTime();
        if(pos > p->clock.currentPosition()) {
            p->clock.sync(pos);
        }
    });
    QObject::connect(p->renderer, &Renderer::atEnd, this, [this]() { p->onRendererFinished(); });
}

void FFmpegEngine::shutdown()
{
    p->killWorkers();

    p->audioOutput->uninit();

    if(p->positionUpdateTimer) {
        p->positionUpdateTimer->deleteLater();
    }
}
} // namespace Fooyin

#include "moc_ffmpegengine.cpp"
