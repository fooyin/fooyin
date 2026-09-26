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

#include "opusdecoder.h"

#include <QLoggingCategory>

#include <cstdio>
#include <limits>

Q_LOGGING_CATEGORY(OPUS_DECODER, "fy.opus.decoder")

using namespace Qt::StringLiterals;

constexpr auto OpusSampleRate = 48000;
constexpr auto MaxHoleRetries = 10;

namespace Fooyin::Opus {
namespace {
AudioFormat::ChannelLayout opusChannelLayout(int channels)
{
    using P = AudioFormat::ChannelPosition;

    switch(channels) {
        case 1:
            return {P::FrontCenter};
        case 2:
            return {P::FrontLeft, P::FrontRight};
        case 3:
            return {P::FrontLeft, P::FrontCenter, P::FrontRight};
        case 4:
            return {P::FrontLeft, P::FrontRight, P::BackLeft, P::BackRight};
        case 5:
            return {P::FrontLeft, P::FrontCenter, P::FrontRight, P::BackLeft, P::BackRight};
        case 6:
            return {P::FrontLeft, P::FrontCenter, P::FrontRight, P::BackLeft, P::BackRight, P::LFE};
        case 7:
            return {P::FrontLeft, P::FrontCenter, P::FrontRight, P::SideLeft, P::SideRight, P::BackCenter, P::LFE};
        case 8:
            return {P::FrontLeft, P::FrontCenter, P::FrontRight, P::SideLeft,
                    P::SideRight, P::BackLeft,    P::BackRight,  P::LFE};
        default:
            return {};
    }
}

QString errorString(int error)
{
    switch(error) {
        case OP_FALSE:
            return u"Request failed"_s;
        case OP_EOF:
            return u"End of stream"_s;
        case OP_HOLE:
            return u"Hole in stream"_s;
        case OP_EREAD:
            return u"Input read failed"_s;
        case OP_EFAULT:
            return u"Internal decoder error"_s;
        case OP_EIMPL:
            return u"Unsupported stream feature"_s;
        case OP_EINVAL:
            return u"Invalid decoder state or argument"_s;
        case OP_ENOTFORMAT:
            return u"Not an Ogg Opus stream"_s;
        case OP_EBADHEADER:
            return u"Invalid Opus header"_s;
        case OP_EVERSION:
            return u"Unsupported Opus version"_s;
        case OP_ENOTAUDIO:
            return u"Stream contains no audio"_s;
        case OP_EBADPACKET:
            return u"Invalid Opus packet"_s;
        case OP_EBADLINK:
            return u"Invalid Opus stream link"_s;
        case OP_ENOSEEK:
            return u"Stream is not seekable"_s;
        case OP_EBADTIMESTAMP:
            return u"Invalid Opus timestamp"_s;
        default:
            return u"Unknown error (%1)"_s.arg(error);
    }
}
} // namespace

OpusDecoder::OpusDecoder()
    : m_decoder{nullptr}
    , m_device{nullptr}
    , m_currentFrame{0}
    , m_totalFrames{0}
    , m_bitrate{0}
    , m_finished{false}
{ }

QStringList OpusDecoder::extensions() const
{
    return {u"opus"_s, u"ogg"_s};
}

bool OpusDecoder::isSeekable() const
{
    return m_decoder && !m_options.testFlag(NoSeeking) && op_seekable(m_decoder.get()) != 0;
}

int OpusDecoder::bitrate() const
{
    return m_bitrate;
}

QStringList OpusDecoder::takeWarnings()
{
    return std::exchange(m_warnings, {});
}

std::optional<AudioFormat> OpusDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    m_device  = source.device;
    m_options = options;

    if(!m_device || !m_device->isOpen()) {
        qCWarning(OPUS_DECODER) << "Unable to initialise Opus decoder for" << track.filepath();
        return {};
    }

    static constexpr OpusFileCallbacks callbacks{
        .read  = readCallback,
        .seek  = seekCallback,
        .tell  = tellCallback,
        .close = nullptr,
    };

    int error{0};
    m_decoder.reset(op_open_callbacks(this, &callbacks, nullptr, 0, &error));
    if(!m_decoder) {
        qCWarning(OPUS_DECODER) << "Unable to open Opus stream" << track.filepath() << ":" << errorString(error);
        stop();
        return {};
    }

    // ReplayGainProcessor accounts for the Opus header gain together with R128/ReplayGain tags
    if(op_set_gain_offset(m_decoder.get(), OP_ABSOLUTE_GAIN, 0) != 0) {
        qCWarning(OPUS_DECODER) << "Unable to configure Opus stream gain for" << track.filepath();
        stop();
        return {};
    }

    const int channels = op_channel_count(m_decoder.get(), 0);
    if(channels <= 0 || channels > AudioFormat::MaxChannels) {
        qCWarning(OPUS_DECODER) << "Unsupported Opus channel count for" << track.filepath() << ":" << channels;
        stop();
        return {};
    }

    m_format = AudioFormat{SampleFormat::F32, OpusSampleRate, channels};
    if(const auto layout = opusChannelLayout(channels); !layout.empty()) {
        m_format.setChannelLayout(layout);
    }
    else {
        m_format.clearChannelLayout();
    }

    const opus_int64 totalFrames = op_pcm_total(m_decoder.get(), -1);
    if(totalFrames >= 0) {
        m_totalFrames = static_cast<uint64_t>(totalFrames);
    }

    const opus_int32 streamBitrate = op_bitrate(m_decoder.get(), -1);
    if(streamBitrate > 0) {
        m_bitrate = streamBitrate / 1000;
    }

    return m_format;
}

void OpusDecoder::stop()
{
    m_decoder.reset();
    m_device  = nullptr;
    m_format  = {};
    m_options = None;
    m_warnings.clear();
    m_currentFrame = 0;
    m_totalFrames  = 0;
    m_bitrate      = 0;
    m_finished     = false;
}

void OpusDecoder::seek(uint64_t pos)
{
    if(!isSeekable()) {
        return;
    }

    uint64_t target = pos > std::numeric_limits<uint64_t>::max() / OpusSampleRate ? std::numeric_limits<uint64_t>::max()
                                                                                  : (pos * OpusSampleRate) / 1000;
    if(m_totalFrames > 0) {
        target = std::min(target, m_totalFrames);
    }
    target = std::min(target, static_cast<uint64_t>(std::numeric_limits<opus_int64>::max()));

    const int result = op_pcm_seek(m_decoder.get(), static_cast<opus_int64>(target));
    if(result < 0) {
        qCWarning(OPUS_DECODER) << "Failed to seek in Opus stream:" << errorString(result);
        return;
    }

    m_currentFrame = target;
    m_finished     = false;
}

AudioDecoder::ReadResult OpusDecoder::readAudio(size_t bytes)
{
    if(!m_decoder || !m_format.isValid()) {
        return ReadResult::errorResult(u"Opus decoder is not initialised"_s);
    }
    if(m_finished) {
        return ReadResult::endOfStream();
    }
    if(abortToken().stop_requested()) {
        return ReadResult::errorResult(u"Opus decoding was cancelled"_s);
    }

    const auto bytesPerFrame = static_cast<size_t>(m_format.bytesPerFrame());
    const auto maxBytes      = static_cast<size_t>(std::numeric_limits<int>::max()) * sizeof(float);
    const size_t requested   = std::min(bytes, maxBytes) - (std::min(bytes, maxBytes) % bytesPerFrame);
    if(requested == 0) {
        return ReadResult::needMoreInput();
    }

    AudioBuffer buffer{m_format, 0};
    buffer.resize(requested);

    int holeRetries{0};
    while(holeRetries <= MaxHoleRetries) {
        int link{-1};
        const int samples = op_read_float(m_decoder.get(), reinterpret_cast<float*>(buffer.data()),
                                          static_cast<int>(requested / sizeof(float)), &link);
        if(samples > 0) {
            const int channels = op_channel_count(m_decoder.get(), link);
            if(channels != m_format.channelCount()) {
                return ReadResult::errorResult(u"Opus stream channel count changed unexpectedly"_s);
            }

            buffer.resize(static_cast<size_t>(samples) * bytesPerFrame);
            buffer.setStartTime(currentTimestamp());
            if(const opus_int64 position = op_pcm_tell(m_decoder.get()); position >= 0) {
                m_currentFrame = static_cast<uint64_t>(position);
            }
            else {
                m_currentFrame += static_cast<uint64_t>(samples);
            }

            const opus_int32 streamBitrate = op_bitrate_instant(m_decoder.get());
            if(streamBitrate > 0) {
                m_bitrate = streamBitrate / 1000;
            }
            return ReadResult::data(std::move(buffer));
        }
        if(samples == 0) {
            m_finished = true;
            return ReadResult::endOfStream();
        }
        if(samples != OP_HOLE || m_options.testFlag(VerifyIntegrity)) {
            return ReadResult::errorResult(u"Failed to decode Opus audio: %1"_s.arg(errorString(samples)));
        }

        ++holeRetries;
        if(const opus_int64 position = op_pcm_tell(m_decoder.get()); position >= 0) {
            m_currentFrame = static_cast<uint64_t>(position);
        }
        if(m_warnings.isEmpty()) {
            m_warnings.append(u"Ignored a hole in the Opus stream"_s);
        }
    }

    return ReadResult::errorResult(u"Failed to decode Opus audio: too many holes in the stream"_s);
}

AudioBuffer OpusDecoder::readBuffer(size_t bytes)
{
    auto result = readAudio(bytes);
    return result.status == ReadStatus::DecodedAudio ? std::move(result.buffer) : AudioBuffer{};
}

int OpusDecoder::readCallback(void* source, unsigned char* buffer, int bytes)
{
    auto* decoder = static_cast<OpusDecoder*>(source);
    if(!decoder || !decoder->m_device || decoder->abortToken().stop_requested() || bytes < 0) {
        return -1;
    }

    return static_cast<int>(decoder->m_device->read(reinterpret_cast<char*>(buffer), bytes));
}

int OpusDecoder::seekCallback(void* source, opus_int64 offset, int whence)
{
    auto* decoder = static_cast<OpusDecoder*>(source);
    if(!decoder || !decoder->m_device || decoder->m_device->isSequential()) {
        return -1;
    }

    qint64 base{0};
    if(whence == SEEK_CUR) {
        base = decoder->m_device->pos();
    }
    else if(whence == SEEK_END) {
        base = decoder->m_device->size();
    }
    else if(whence != SEEK_SET) {
        return -1;
    }

    if(base < 0 || !std::in_range<qint64>(offset)) {
        return -1;
    }

    const auto relative = static_cast<qint64>(offset);
    if((relative > 0 && base > std::numeric_limits<qint64>::max() - relative) || relative < -base) {
        return -1;
    }

    return decoder->m_device->seek(base + relative) ? 0 : -1;
}

opus_int64 OpusDecoder::tellCallback(void* source)
{
    auto* decoder = static_cast<OpusDecoder*>(source);
    if(!decoder || !decoder->m_device) {
        return -1;
    }

    return decoder->m_device->pos();
}

uint64_t OpusDecoder::currentTimestamp() const
{
    return ((m_currentFrame / OpusSampleRate) * 1000) + (((m_currentFrame % OpusSampleRate) * 1000) / OpusSampleRate);
}
} // namespace Fooyin::Opus
