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

#include "ffmpegclock.h"
#include "ffmpegframe.h"
#include "ffmpegpacket.h"

#include <QDebug>

#include <deque>

namespace Fy::Core::Engine::FFmpeg {
constexpr int MaxPendingFrames = 9;

struct Decoder::Private
{
    Decoder* decoder;
    AVFormatContext* context;
    const Codec* codec;
    int streamIndex;
    AudioClock* clock;

    int pendingFramesCount;
    std::deque<Packet> packets;

    Private(Decoder* decoder, AVFormatContext* context, const Codec& codec, int streamIndex, AudioClock* clock)
        : decoder{decoder}
        , context{context}
        , codec{&codec}
        , streamIndex{streamIndex}
        , clock{clock}
        , pendingFramesCount{0}
    { }

    void reset()
    {
        packets.clear();
        pendingFramesCount = 0;
    }

    void decodeAudio(const Packet& packet)
    {
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

    void onFrameFound(Frame frame)
    {
        ++pendingFramesCount;
        emit decoder->requestHandleFrame(frame);
    }

    int sendAVPacket(const Packet& packet)
    {
        return avcodec_send_packet(codec->context(), packet.isValid() ? packet.avPacket() : nullptr);
    }

    void receiveAVFrames()
    {
        while(true) {
            auto avFrame     = FramePtr(av_frame_alloc());
            const int result = avcodec_receive_frame(codec->context(), avFrame.get());

            if(result == AVERROR_EOF || result == AVERROR(EAGAIN)) {
                break;
            }
            if(result < 0) {
                qWarning() << "Error receiving decoded frame";
                break;
            }
            avFrame->time_base = context->streams[streamIndex]->time_base;
            onFrameFound({std::move(avFrame), *codec});
        }
    }
};

Decoder::Decoder(AVFormatContext* context, const Codec& codec, int streamIndex, AudioClock* clock, QObject* parent)
    : EngineWorker{parent}
    , p{std::make_unique<Private>(this, context, codec, streamIndex, clock)}
{
    setObjectName("Decoder");
}

Decoder::~Decoder() = default;

void Decoder::seek(uint64_t position)
{
    const int flags = position < p->clock->currentPosition() ? AVSEEK_FLAG_BACKWARD : 0;
    if(av_seek_frame(p->context, 0, position, flags) < 0) {
        qWarning() << "Could not seek to position: " << position;
        return;
    }
    p->reset();
    avcodec_flush_buffers(p->codec->context());
    scheduleNextStep();
}

void Decoder::decode(Packet& packet)
{
    p->packets.emplace_back(std::move(packet));
    scheduleNextStep();
}

void Decoder::onFrameProcessed()
{
    --p->pendingFramesCount;
    scheduleNextStep(false);
}

bool Decoder::canDoNextStep() const
{
    return p->pendingFramesCount < MaxPendingFrames && EngineWorker::canDoNextStep();
}

void Decoder::doNextStep()
{
    Packet packet(PacketPtr{av_packet_alloc()});
    if(av_read_frame(p->context, packet.avPacket()) < 0) {
        Frame frame;
        emit requestHandleFrame(frame);
        return;
    }

    if(packet.avPacket()->stream_index != p->streamIndex) {
        return scheduleNextStep(false);
    }

    p->decodeAudio(packet);
    scheduleNextStep(false);
}
} // namespace Fy::Core::Engine::FFmpeg
