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

#include "vorbisdecoder.h"

#include <QLoggingCategory>

#include <cstdio>
#include <limits>

Q_LOGGING_CATEGORY(VORBIS_DECODER, "fy.vorbis.decoder")

using namespace Qt::StringLiterals;

constexpr auto MaxHoleRetries = 10;

namespace Fooyin::Vorbis {
namespace {
AudioFormat::ChannelLayout vorbisChannelLayout(int channels)
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
        case OV_FALSE:
            return u"Request failed"_s;
        case OV_EOF:
            return u"End of stream"_s;
        case OV_HOLE:
            return u"Hole in stream"_s;
        case OV_EREAD:
            return u"Input read failed"_s;
        case OV_EFAULT:
            return u"Internal decoder error"_s;
        case OV_EIMPL:
            return u"Unsupported stream feature"_s;
        case OV_EINVAL:
            return u"Invalid decoder state or argument"_s;
        case OV_ENOTVORBIS:
            return u"Not an Ogg Vorbis stream"_s;
        case OV_EBADHEADER:
            return u"Invalid Vorbis header"_s;
        case OV_EVERSION:
            return u"Unsupported Vorbis version"_s;
        case OV_ENOTAUDIO:
            return u"Stream contains no audio"_s;
        case OV_EBADPACKET:
            return u"Invalid Vorbis packet"_s;
        case OV_EBADLINK:
            return u"Invalid Vorbis stream link"_s;
        case OV_ENOSEEK:
            return u"Stream is not seekable"_s;
        default:
            return u"Unknown error (%1)"_s.arg(error);
    }
}
} // namespace

VorbisDecoder::VorbisDecoder()
    : m_decoder{nullptr}
    , m_device{nullptr}
    , m_currentFrame{0}
    , m_totalFrames{0}
    , m_bitrate{0}
    , m_finished{false}
{ }

QStringList VorbisDecoder::extensions() const
{
    return {u"ogg"_s, u"oga"_s};
}

bool VorbisDecoder::isSeekable() const
{
    return m_decoder && ov_seekable(m_decoder.get()) != 0;
}

int VorbisDecoder::bitrate() const
{
    return m_bitrate;
}

QStringList VorbisDecoder::takeWarnings()
{
    return std::exchange(m_warnings, {});
}

std::optional<AudioFormat> VorbisDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    m_device  = source.device;
    m_options = options;

    if(!m_device || !m_device->isOpen()) {
        qCWarning(VORBIS_DECODER) << "Unable to initialise Vorbis decoder for" << track.filepath();
        return {};
    }

    const ov_callbacks callbacks{
        .read_func  = readCallback,
        .seek_func  = seekCallback,
        .close_func = nullptr,
        .tell_func  = tellCallback,
    };

    auto decoder    = std::make_unique<OggVorbis_File>();
    const int error = ov_open_callbacks(this, decoder.get(), nullptr, 0, callbacks);
    if(error < 0) {
        qCWarning(VORBIS_DECODER) << "Unable to open Vorbis stream" << track.filepath() << ":" << errorString(error);
        stop();
        return {};
    }
    m_decoder.reset(decoder.release());

    const vorbis_info* info = ov_info(m_decoder.get(), -1);
    if(!info || info->rate <= 0 || info->channels <= 0 || info->channels > AudioFormat::MaxChannels) {
        qCWarning(VORBIS_DECODER) << "Vorbis stream has invalid audio properties for" << track.filepath();
        stop();
        return {};
    }

    m_format = AudioFormat{SampleFormat::F32, static_cast<int>(info->rate), info->channels};
    if(const auto layout = vorbisChannelLayout(info->channels); !layout.empty()) {
        m_format.setChannelLayout(layout);
    }
    else {
        m_format.clearChannelLayout();
    }

    if(isSeekable()) {
        const auto links = static_cast<int>(ov_streams(m_decoder.get()));
        for(int link{0}; link < links; ++link) {
            if(!formatMatches(link)) {
                // TODO: Support format changes in engine
                qCWarning(VORBIS_DECODER) << "Vorbis stream changes format between links for" << track.filepath();
                stop();
                return {};
            }
        }
    }

    const ogg_int64_t totalFrames = ov_pcm_total(m_decoder.get(), -1);
    if(totalFrames >= 0) {
        m_totalFrames = static_cast<uint64_t>(totalFrames);
    }

    const long streamBitrate = ov_bitrate(m_decoder.get(), -1);
    if(streamBitrate > 0) {
        m_bitrate = static_cast<int>(streamBitrate / 1000);
    }

    return m_format;
}

void VorbisDecoder::stop()
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

void VorbisDecoder::seek(uint64_t pos)
{
    const auto sampleRate = static_cast<uint64_t>(m_format.sampleRate());

    uint64_t target = pos > std::numeric_limits<uint64_t>::max() / sampleRate ? std::numeric_limits<uint64_t>::max()
                                                                              : (pos * sampleRate) / 1000;
    if(m_totalFrames > 0) {
        target = std::min(target, m_totalFrames);
    }
    target = std::min(target, static_cast<uint64_t>(std::numeric_limits<ogg_int64_t>::max()));

    const int result = ov_pcm_seek(m_decoder.get(), static_cast<ogg_int64_t>(target));
    if(result < 0) {
        qCWarning(VORBIS_DECODER) << "Failed to seek in Vorbis stream:" << errorString(result);
        return;
    }

    m_currentFrame = target;
    m_finished     = false;
}

AudioDecoder::ReadResult VorbisDecoder::readAudio(size_t bytes)
{
    if(!m_decoder || !m_format.isValid()) {
        return ReadResult::errorResult(u"Vorbis decoder is not initialised"_s);
    }
    if(m_finished) {
        return ReadResult::endOfStream();
    }
    if(abortToken().stop_requested()) {
        return ReadResult::errorResult(u"Vorbis decoding was cancelled"_s);
    }

    const auto bytesPerFrame = static_cast<size_t>(m_format.bytesPerFrame());
    const size_t requested   = bytes - (bytes % bytesPerFrame);
    if(requested == 0) {
        return ReadResult::needMoreInput();
    }

    const size_t requestedFrames
        = std::min(requested / bytesPerFrame, static_cast<size_t>(std::numeric_limits<int>::max()));

    int holeRetries{0};
    while(holeRetries <= MaxHoleRetries) {
        float** channels{nullptr};
        int link{-1};

        const long samples = ov_read_float(m_decoder.get(), &channels, static_cast<int>(requestedFrames), &link);
        if(samples > 0) {
            if(!formatMatches(link)) {
                return ReadResult::errorResult(u"Vorbis stream format changed unexpectedly"_s);
            }

            const auto frameCount = static_cast<size_t>(samples);
            AudioBuffer buffer{m_format, currentTimestamp()};
            buffer.resize(frameCount * bytesPerFrame);

            auto* output           = reinterpret_cast<float*>(buffer.data());
            const int channelCount = m_format.channelCount();
            for(size_t frame{0}; frame < frameCount; ++frame) {
                for(int channel{0}; channel < channelCount; ++channel) {
                    *output++ = channels[channel][frame];
                }
            }

            if(const ogg_int64_t position = ov_pcm_tell(m_decoder.get()); position >= 0) {
                m_currentFrame = static_cast<uint64_t>(position);
            }
            else {
                m_currentFrame += frameCount;
            }

            const long streamBitrate = ov_bitrate_instant(m_decoder.get());
            if(streamBitrate > 0) {
                m_bitrate = static_cast<int>(streamBitrate / 1000);
            }
            return ReadResult::data(std::move(buffer));
        }
        if(samples == 0) {
            m_finished = true;
            return ReadResult::endOfStream();
        }
        if(samples != OV_HOLE || m_options.testFlag(VerifyIntegrity)) {
            return ReadResult::errorResult(
                u"Failed to decode Vorbis audio: %1"_s.arg(errorString(static_cast<int>(samples))));
        }

        ++holeRetries;
        if(const ogg_int64_t position = ov_pcm_tell(m_decoder.get()); position >= 0) {
            m_currentFrame = static_cast<uint64_t>(position);
        }
        if(m_warnings.isEmpty()) {
            m_warnings.append(u"Ignored a hole in the Vorbis stream"_s);
        }
    }

    return ReadResult::errorResult(u"Failed to decode Vorbis audio: too many holes in the stream"_s);
}

AudioBuffer VorbisDecoder::readBuffer(size_t bytes)
{
    auto result = readAudio(bytes);
    return result.status == ReadStatus::DecodedAudio ? std::move(result.buffer) : AudioBuffer{};
}

size_t VorbisDecoder::readCallback(void* buffer, size_t size, size_t count, void* source)
{
    auto* decoder = static_cast<VorbisDecoder*>(source);
    if(!decoder || !decoder->m_device || decoder->abortToken().stop_requested() || size == 0 || count == 0) {
        return 0;
    }

    const size_t requestedBytes = size * count;
    if(!std::in_range<qint64>(requestedBytes)) {
        return 0;
    }

    const auto requested = static_cast<qint64>(requestedBytes);
    const qint64 read    = decoder->m_device->read(static_cast<char*>(buffer), requested);
    return read > 0 ? static_cast<size_t>(read) / size : 0;
}

int VorbisDecoder::seekCallback(void* source, ogg_int64_t offset, int whence)
{
    auto* decoder = static_cast<VorbisDecoder*>(source);
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

long VorbisDecoder::tellCallback(void* source)
{
    auto* decoder = static_cast<VorbisDecoder*>(source);
    if(!decoder || !decoder->m_device) {
        return -1;
    }

    const qint64 position = decoder->m_device->pos();
    if(!std::in_range<long>(position)) {
        return -1;
    }
    return static_cast<long>(position);
}

bool VorbisDecoder::formatMatches(int link) const
{
    const vorbis_info* info = ov_info(m_decoder.get(), link);
    return info && info->rate == m_format.sampleRate() && info->channels == m_format.channelCount();
}

uint64_t VorbisDecoder::currentTimestamp() const
{
    const auto sampleRate = static_cast<uint64_t>(m_format.sampleRate());
    return ((m_currentFrame / sampleRate) * 1000) + (((m_currentFrame % sampleRate) * 1000) / sampleRate);
}
} // namespace Fooyin::Vorbis
