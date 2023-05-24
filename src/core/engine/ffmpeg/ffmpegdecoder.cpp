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

#include "ffmpegdecoder.h"

// #include "ffmpegaudiobuffer.h"
#include "ffmpegcodec.h"
#include "ffmpegframe.h"
#include "ffmpegpacket.h"
#include "ffmpegutils.h"

#include <QDebug>

#include <bit>
#include <deque>

namespace Fy::Core::Engine::FFmpeg {
constexpr int MaxFrameQueue = 9;

// Copies input frame's metadata and properties and assigns empty buffers for the data
Frame copyFrame(const Frame& frame, AVSampleFormat format)
{
    auto avFrame = FramePtr(av_frame_alloc());

    av_frame_copy_props(avFrame.get(), frame.avFrame());
    avFrame->ch_layout  = frame.avFrame()->ch_layout;
    avFrame->nb_samples = frame.sampleCount();
    avFrame->format     = format;

    Frame newFrame{std::move(avFrame)};

    const int err = av_frame_get_buffer(newFrame.avFrame(), 0);
    if(err < 0) {
        qWarning() << "Buffer could not be allocated for frame";
        return {};
    }
    return newFrame;
}

struct Decoder::Private
{
    Decoder* decoder;
    AVFormatContext* context;
    Codec* codec;

    int pendingFrameCount{0};

    Private(Decoder* decoder)
        : decoder{decoder}
        , context{nullptr}
        , codec{nullptr}
    { }

    void decodeAudio(const Packet& packet)
    {
        if(decoder->isPaused()) {
            return;
        }

        int result = sendAVPacket(packet);

        if(result == AVERROR(EAGAIN)) {
            receiveAVFrames();
            result = sendAVPacket(packet);

            if(result != AVERROR(EAGAIN)) {
                qWarning() << "Unexpected decoder behavior";
            }
        }

        if(result == 0) {
            receiveAVFrames();
        }
    }

    int sendAVPacket(const Packet& packet) const
    {
        return avcodec_send_packet(codec->context(), packet.avPacket());
    }

    void receiveAVFrames()
    {
        if(decoder->isPaused()) {
            return;
        }

        auto avFrame     = FramePtr(av_frame_alloc());
        const int result = avcodec_receive_frame(codec->context(), avFrame.get());

        if(result == AVERROR_EOF || result == AVERROR(EAGAIN)) {
            return;
        }
        if(result < 0) {
            qWarning() << "Error receiving decoded frame";
            return;
        }
        avFrame->time_base = context->streams[codec->streamIndex()]->time_base;
        Frame frame{std::move(avFrame)};

        if(av_sample_fmt_is_planar(frame.format())) {
            frame = interleave(frame);
        }
        ++pendingFrameCount;
        emit decoder->requestHandleFrame(frame);
    }

    template <typename T>
    void interleaveSamples(uint8_t** in, int channels, uint8_t* out, int frames)
    {
        for(int ch = 0; ch < channels; ++ch) {
            auto pSamples = std::bit_cast<const T*>(in[ch]);
            auto iSamples = std::bit_cast<T*>(out) + ch;
            auto end      = pSamples + frames;
            while(pSamples < end) {
                *iSamples = *pSamples++;
                iSamples += channels;
            }
        }
    }

    Frame interleave(const Frame& inputFrame)
    {
        uint8_t** in                 = inputFrame.avFrame()->data;
        const int channels           = inputFrame.channelCount();
        const int samples            = inputFrame.sampleCount();
        const auto interleavedFormat = interleaveFormat(inputFrame.format());

        Frame frame  = copyFrame(inputFrame, interleavedFormat);
        uint8_t* out = frame.avFrame()->data[0];

        switch(inputFrame.format()) {
            case AV_SAMPLE_FMT_FLTP:
                interleaveSamples<float>(in, channels, out, samples);
                break;
            case AV_SAMPLE_FMT_U8P:
                interleaveSamples<int8_t>(in, channels, out, samples);
                break;
            case AV_SAMPLE_FMT_S16P:
                interleaveSamples<int16_t>(in, channels, out, samples);
                break;
            case AV_SAMPLE_FMT_S32P:
                interleaveSamples<int32_t>(in, channels, out, samples);
                break;
            case AV_SAMPLE_FMT_S64P:
            case AV_SAMPLE_FMT_DBLP:
                interleaveSamples<int64_t>(in, channels, out, samples);
                break;
            case AV_SAMPLE_FMT_NONE:
            case AV_SAMPLE_FMT_U8:
            case AV_SAMPLE_FMT_S16:
            case AV_SAMPLE_FMT_S32:
            case AV_SAMPLE_FMT_FLT:
            case AV_SAMPLE_FMT_DBL:
            case AV_SAMPLE_FMT_S64:
            case AV_SAMPLE_FMT_NB:
                break;
        }
        frame.avFrame()->format = interleavedFormat;
        return frame;
    }
};

Decoder::Decoder(QObject* parent)
    : EngineWorker{parent}
    , p{std::make_unique<Private>(this)}
{
    setObjectName("Decoder");
}

Decoder::~Decoder() = default;

void Decoder::run(AVFormatContext* context, Codec* codec)
{
    p->context = context;
    p->codec   = codec;
    setPaused(false);
    scheduleNextStep();
}

void Decoder::reset()
{
    EngineWorker::reset();
    p->pendingFrameCount = 0;
}

void Decoder::kill()
{
    EngineWorker::kill();
    p->pendingFrameCount = 0;

    p->context = nullptr;
    p->codec   = nullptr;
}

void Decoder::onFrameProcessed()
{
    --p->pendingFrameCount;
    scheduleNextStep(false);
}

bool Decoder::canDoNextStep() const
{
    return p->pendingFrameCount < MaxFrameQueue && EngineWorker::canDoNextStep();
}

int Decoder::timerInterval() const
{
    return 5;
}

void Decoder::doNextStep()
{
    if(isPaused()) {
        return;
    }
    const Packet packet(PacketPtr{av_packet_alloc()});
    if(av_read_frame(p->context, packet.avPacket()) < 0) {
        // Invalid EOF frame
        emit requestHandleFrame({});
        p->decoder->setAtEnd(true);
        return;
    }

    if(packet.avPacket()->stream_index != p->codec->streamIndex()) {
        return scheduleNextStep(false);
    }

    packet.avPacket()->time_base = p->codec->stream()->time_base;

    p->decodeAudio(packet);
    scheduleNextStep(false);
}
} // namespace Fy::Core::Engine::FFmpeg
