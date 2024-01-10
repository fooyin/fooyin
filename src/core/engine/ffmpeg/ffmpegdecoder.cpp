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

#include "ffmpegdecoder.h"

#include "ffmpegcodec.h"
#include "ffmpegframe.h"
#include "ffmpegpacket.h"
#include "ffmpegutils.h"

#include <QDebug>

#include <bit>

constexpr int MaxFrameQueue = 9;

namespace {
// Copies input frame's metadata and properties and assigns empty buffers for the data
Fooyin::Frame copyFrame(const Fooyin::Frame& frame, AVSampleFormat format)
{
    auto avFrame = Fooyin::FramePtr(av_frame_alloc());

    av_frame_copy_props(avFrame.get(), frame.avFrame());
    avFrame->ch_layout  = frame.avFrame()->ch_layout;
    avFrame->nb_samples = frame.sampleCount();
    avFrame->format     = format;

    Fooyin::Frame newFrame{std::move(avFrame)};

    const int err = av_frame_get_buffer(newFrame.avFrame(), 0);
    if(err < 0) {
        qWarning() << "Buffer could not be allocated for frame";
        return {};
    }
    return newFrame;
}

template <typename T>
void interleaveSamples(uint8_t** in, int channels, uint8_t* out, int frames)
{
    for(int ch = 0; ch < channels; ++ch) {
        const auto* pSamples = std::bit_cast<const T*>(in[ch]);
        auto* iSamples       = std::bit_cast<T*>(out) + ch;
        auto end             = pSamples + frames;
        while(pSamples < end) {
            *iSamples = *pSamples++;
            iSamples += channels;
        }
    }
}

Fooyin::Frame interleave(const Fooyin::Frame& inputFrame)
{
    uint8_t** in                 = inputFrame.avFrame()->data;
    const int channels           = inputFrame.channelCount();
    const int samples            = inputFrame.sampleCount();
    const auto interleavedFormat = Fooyin::Utils::interleaveFormat(inputFrame.format());

    Fooyin::Frame frame = copyFrame(inputFrame, interleavedFormat);
    uint8_t* out    = frame.avFrame()->data[0];

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
} // namespace

namespace Fooyin {
struct Decoder::Private
{
    Decoder* decoder;
    AVFormatContext* context{nullptr};
    Codec* codec{nullptr};

    int pendingFrameCount{0};
    bool draining{false};

    explicit Private(Decoder* decoder)
        : decoder{decoder}
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

    bool checkCodecContext() const
    {
        return context && codec && codec->context();
    }

    int sendAVPacket(const Packet& packet) const
    {
        if(checkCodecContext() && !decoder->isPaused()) {
            return avcodec_send_packet(codec->context(), !packet.isValid() || draining ? nullptr : packet.avPacket());
        }
        return -1;
    }

    void receiveAVFrames()
    {
        if(decoder->isPaused()) {
            return;
        }

        if(!checkCodecContext()) {
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
    p->draining          = false;
}

void Decoder::kill()
{
    EngineWorker::kill();
    p->pendingFrameCount = 0;
    p->draining          = false;
    p->context           = nullptr;
    p->codec             = nullptr;
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
    if(isPaused() || !p->checkCodecContext()) {
        return;
    }

    const Packet packet(PacketPtr{av_packet_alloc()});
    const int readResult = av_read_frame(p->context, packet.avPacket());
    if(readResult < 0) {
        if(readResult != AVERROR_EOF) {
            Utils::printError(readResult);
        }
        else if(!p->draining) {
            p->draining = true;
            p->decodeAudio(packet);
            scheduleNextStep(false);
            return;
        }

        emit requestHandleFrame({});
        p->decoder->setAtEnd(true);
        return;
    }

    if(packet.avPacket()->stream_index != p->codec->streamIndex()) {
        scheduleNextStep(false);
        return;
    }

    packet.avPacket()->time_base = p->codec->stream()->time_base;

    p->decodeAudio(packet);
    scheduleNextStep(false);
}
} // namespace Fooyin

#include "moc_ffmpegdecoder.cpp"
