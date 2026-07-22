/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#pragma once

#include "fycore_export.h"

#include "ffmpegutils.h"

#include <core/engine/audioencoder.h>

namespace Fooyin {
class FYCORE_EXPORT FFmpegEncoder : public AudioEncoder
{
public:
    FFmpegEncoder();
    ~FFmpegEncoder() override;

    [[nodiscard]] std::vector<AudioEncoderInfo> availableEncoders() const override;

    Result init(const QString& outputPath, const AudioFormat& inputFormat,
                const AudioEncoderSettings& settings) override;
    Result write(const AudioBuffer& buffer) override;
    Result finish() override;

private:
    Result configureResampler();
    Result encodeInput(const uint8_t* inputData, int inputFrames);
    Result queueConvertedAudio(AVFrame* frame, int frames);
    Result encodeQueuedFrames(bool final);
    Result flushResampler();
    Result prepareFrame(AVFrame* frame, int frames) const;
    Result encodeFrame(AVFrame* frame);
    Result drainPackets();
    void cleanup();

    OutputFormatContextPtr m_formatContext;
    CodecContextPtr m_codecContext;
    AVStream* m_stream;
    SwrContextPtr m_swr;
    FramePtr m_frame;
    FramePtr m_convertedFrame;
    PacketPtr m_packet;
    AudioFifoPtr m_fifo;
    AudioFormat m_inputFormat;
    AVSampleFormat m_inputSampleFormat;
    AVSampleFormat m_outputSampleFormat;
    int m_outputSampleRate;
    int64_t m_nextPts;
    bool m_dither;
    bool m_finished;
};
} // namespace Fooyin
