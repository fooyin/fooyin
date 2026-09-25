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

#include "flacdecoder.h"

#include <QIODevice>
#include <QLoggingCategory>

#include <cstring>
#include <limits>

Q_LOGGING_CATEGORY(FLAC_DECODER, "fy.flac.decoder")

using namespace Qt::StringLiterals;

namespace Fooyin::Flac {
namespace {
SampleFormat sampleFormatForBits(unsigned bps)
{
    if(bps <= 16) {
        return SampleFormat::S16;
    }
    if(bps <= 24) {
        return SampleFormat::S24In32;
    }
    return SampleFormat::S32;
}

AudioFormat::ChannelLayout flacChannelLayout(unsigned channels)
{
    using P = AudioFormat::ChannelPosition;
    if(channels == 7) {
        return {P::FrontLeft, P::FrontRight, P::FrontCenter, P::LFE, P::BackCenter, P::SideLeft, P::SideRight};
    }
    return AudioFormat::defaultChannelLayoutForChannelCount(static_cast<int>(channels));
}
} // namespace

FlacDecoder::FlacDecoder()
    : m_decoder{nullptr}
    , m_device{nullptr}
    , m_pendingOffset{0}
    , m_currentFrame{0}
    , m_totalFrames{0}
    , m_initialised{false}
    , m_finished{false}
{ }

QStringList FlacDecoder::extensions() const
{
    return {u"flac"_s};
}

bool FlacDecoder::isSeekable() const
{
    return m_initialised && !m_options.testFlag(NoSeeking) && m_device && !m_device->isSequential();
}

std::optional<AudioFormat> FlacDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    m_device  = source.device;
    m_options = options;

    {
        FLAC__StreamDecoder* rawDecoder = FLAC__stream_decoder_new();
        if(!rawDecoder) {
            qCWarning(FLAC_DECODER) << "Failed to allocate FLAC decoder for" << track.filepath();
            return {};
        }
        m_decoder.reset(rawDecoder);
    }
    FLAC__StreamDecoder* decoder = m_decoder.get();

    FLAC__stream_decoder_set_md5_checking(decoder, options.testFlag(VerifyIntegrity));

    const auto initStatus
        = FLAC__stream_decoder_init_stream(decoder, readCallback, seekCallback, tellCallback, lengthCallback,
                                           eofCallback, writeCallback, metadataCallback, errorCallback, this);
    if(initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        qCWarning(FLAC_DECODER) << "Failed to initialise FLAC decoder for" << track.filepath() << ":"
                                << FLAC__StreamDecoderInitStatusString[initStatus];
        stop();
        return {};
    }

    m_initialised = true;
    if(!FLAC__stream_decoder_process_until_end_of_metadata(decoder) || !m_format.isValid() || !m_error.isEmpty()) {
        qCWarning(FLAC_DECODER) << "Failed to read FLAC stream information for" << track.filepath();
        return {};
    }

    return m_format;
}

void FlacDecoder::stop()
{
    if(m_decoder) {
        if(m_initialised && FLAC__stream_decoder_get_state(m_decoder.get()) != FLAC__STREAM_DECODER_UNINITIALIZED) {
            FLAC__stream_decoder_finish(m_decoder.get());
        }
    }

    m_decoder.reset();
    m_device  = nullptr;
    m_format  = {};
    m_options = None;
    m_pending.clear();
    m_pendingOffset = 0;
    m_currentFrame  = 0;
    m_totalFrames   = 0;
    m_error.clear();
    m_warnings.clear();
    m_initialised = false;
    m_finished    = false;
}

void FlacDecoder::seek(uint64_t pos)
{
    if(!isSeekable() || !m_decoder || m_format.sampleRate() <= 0) {
        return;
    }

    const auto rate       = static_cast<uint64_t>(m_format.sampleRate());
    const uint64_t sample = ((pos / 1000) * rate) + (((pos % 1000) * rate) / 1000);
    const uint64_t target = m_totalFrames > 0 ? std::min(sample, m_totalFrames) : sample;

    m_pending.clear();
    m_pendingOffset = 0;
    m_error.clear();

    if(FLAC__stream_decoder_seek_absolute(m_decoder.get(), target)) {
        m_currentFrame = target;
        m_finished     = false;
    }
    else {
        setError(u"Failed to seek in FLAC stream"_s);
    }
}

AudioDecoder::ReadResult FlacDecoder::readAudio(size_t bytes)
{
    if(!m_initialised || !m_decoder) {
        return ReadResult::errorResult(u"FLAC decoder is not initialised"_s);
    }
    if(!m_error.isEmpty()) {
        return ReadResult::errorResult(m_error);
    }
    if(m_finished) {
        return ReadResult::endOfStream();
    }
    if(abortToken().stop_requested()) {
        return ReadResult::errorResult(u"FLAC decoding was cancelled"_s);
    }

    const auto bytesPerFrame = static_cast<size_t>(m_format.bytesPerFrame());
    const size_t aligned     = bytes - (bytes % bytesPerFrame);
    if(aligned == 0) {
        return ReadResult::needMoreInput();
    }

    static constexpr auto maxRequest = static_cast<size_t>(std::numeric_limits<int>::max());
    const size_t requested           = std::min(aligned, maxRequest - (maxRequest % bytesPerFrame));

    AudioBuffer output{m_format, currentTimestamp()};
    output.reserve(requested);

    while(std::cmp_less(output.byteCount(), requested)) {
        if(m_pendingOffset < m_pending.size()) {
            const size_t available = m_pending.size() - m_pendingOffset;
            const size_t remaining = requested - static_cast<size_t>(output.byteCount());
            const size_t count     = std::min(available, remaining);
            output.append(m_pending.data() + m_pendingOffset, count);
            m_pendingOffset += count;
            m_currentFrame += count / bytesPerFrame;

            if(m_pendingOffset == m_pending.size()) {
                m_pending.clear();
                m_pendingOffset = 0;
            }
            continue;
        }

        if(FLAC__stream_decoder_get_state(m_decoder.get()) == FLAC__STREAM_DECODER_END_OF_STREAM) {
            if(!finishStream()) {
                break;
            }
            break;
        }

        if(!FLAC__stream_decoder_process_single(m_decoder.get())) {
            if(m_error.isEmpty()) {
                const auto state = FLAC__stream_decoder_get_state(m_decoder.get());
                setError(u"Failed to decode FLAC audio: %1"_s.arg(FLAC__StreamDecoderStateString[state]));
            }
            break;
        }

        if(!m_error.isEmpty()) {
            break;
        }
    }

    if(output.isValid()) {
        return ReadResult::data(std::move(output));
    }
    if(!m_error.isEmpty()) {
        return ReadResult::errorResult(m_error);
    }
    return ReadResult::endOfStream();
}

AudioBuffer FlacDecoder::readBuffer(size_t bytes)
{
    auto result = readAudio(bytes);
    return result.status == ReadStatus::DecodedAudio ? std::move(result.buffer) : AudioBuffer{};
}

QStringList FlacDecoder::takeWarnings()
{
    return std::exchange(m_warnings, {});
}

FLAC__StreamDecoderReadStatus FlacDecoder::readCallback(const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[],
                                                        size_t* bytes, void* clientData)
{
    auto* decoder = static_cast<FlacDecoder*>(clientData);
    if(!decoder || decoder->abortToken().stop_requested()) {
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    const qint64 read = decoder->m_device->read(reinterpret_cast<char*>(buffer), static_cast<qint64>(*bytes));
    if(read > 0) {
        *bytes = static_cast<size_t>(read);
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }

    *bytes = 0;
    return read == 0 ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

FLAC__StreamDecoderSeekStatus FlacDecoder::seekCallback(const FLAC__StreamDecoder* /*decoder*/, FLAC__uint64 offset,
                                                        void* clientData)
{
    auto* decoder = static_cast<FlacDecoder*>(clientData);
    if(!decoder || decoder->m_device->isSequential()) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
    }

    if(offset > static_cast<FLAC__uint64>(std::numeric_limits<qint64>::max())) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }

    return decoder->m_device->seek(static_cast<qint64>(offset)) ? FLAC__STREAM_DECODER_SEEK_STATUS_OK
                                                                : FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
}

FLAC__StreamDecoderTellStatus FlacDecoder::tellCallback(const FLAC__StreamDecoder* /*decoder*/, FLAC__uint64* offset,
                                                        void* clientData)
{
    auto* decoder = static_cast<FlacDecoder*>(clientData);
    if(!decoder || decoder->m_device->isSequential()) {
        return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
    }

    const qint64 pos = decoder->m_device->pos();
    if(pos < 0) {
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    }

    *offset = static_cast<FLAC__uint64>(pos);
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus FlacDecoder::lengthCallback(const FLAC__StreamDecoder* /*decoder*/,
                                                            FLAC__uint64* length, void* clientData)
{
    auto* decoder = static_cast<FlacDecoder*>(clientData);
    if(!decoder || decoder->m_device->isSequential()) {
        return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
    }

    const qint64 size = decoder->m_device->size();
    if(size < 0) {
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    }

    *length = static_cast<FLAC__uint64>(size);
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool FlacDecoder::eofCallback(const FLAC__StreamDecoder* /*decoder*/, void* clientData)
{
    const auto* decoder = static_cast<const FlacDecoder*>(clientData);
    return decoder && decoder->m_device->atEnd();
}

FLAC__StreamDecoderWriteStatus FlacDecoder::writeCallback(const FLAC__StreamDecoder* /*decoder*/,
                                                          const FLAC__Frame* frame, const FLAC__int32* const buffer[],
                                                          void* clientData)
{
    auto* decoder = static_cast<FlacDecoder*>(clientData);
    if(!decoder || !decoder->m_format.isValid() || decoder->abortToken().stop_requested()) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    const uint32_t channels      = frame->header.channels;
    const uint32_t bitsPerSample = frame->header.bits_per_sample;
    const uint32_t blockSize     = frame->header.blocksize;
    if(std::cmp_not_equal(channels, decoder->m_format.channelCount()) || bitsPerSample == 0 || bitsPerSample > 32) {
        decoder->setError(u"FLAC stream format changed unexpectedly"_s);
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    const auto bytesPerSample = static_cast<size_t>(decoder->m_format.bytesPerSample());
    decoder->m_pending.resize(static_cast<size_t>(blockSize) * channels * bytesPerSample);
    decoder->m_pendingOffset = 0;

    std::byte* dest           = decoder->m_pending.data();
    const uint32_t outputBits = decoder->m_format.sampleFormat() == SampleFormat::S16 ? 16 : 32;
    const uint32_t shift      = outputBits - bitsPerSample;

    for(uint32_t sample{0}; sample < blockSize; ++sample) {
        for(uint32_t channel{0}; channel < channels; ++channel) {
            const int64_t scaled = static_cast<int64_t>(buffer[channel][sample]) * (int64_t{1} << shift);
            if(outputBits == 16) {
                const auto value = static_cast<int16_t>(scaled);
                std::memcpy(dest, &value, sizeof(value));
                dest += sizeof(value);
            }
            else {
                const auto value = static_cast<int32_t>(scaled);
                std::memcpy(dest, &value, sizeof(value));
                dest += sizeof(value);
            }
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacDecoder::metadataCallback(const FLAC__StreamDecoder* /*decoder*/, const FLAC__StreamMetadata* metadata,
                                   void* clientData)
{
    auto* decoder = static_cast<FlacDecoder*>(clientData);
    if(!decoder || metadata->type != FLAC__METADATA_TYPE_STREAMINFO) {
        return;
    }

    const auto& info = metadata->data.stream_info; // NOLINT
    if(info.sample_rate == 0 || info.channels == 0 || info.channels > 8 || info.bits_per_sample == 0) {
        decoder->setError(u"FLAC stream has invalid audio properties"_s);
        return;
    }

    decoder->m_format = {sampleFormatForBits(info.bits_per_sample), static_cast<int>(info.sample_rate),
                         static_cast<int>(info.channels)};
    decoder->m_format.setChannelLayout(flacChannelLayout(info.channels));
    decoder->m_totalFrames = info.total_samples;
}

void FlacDecoder::errorCallback(const FLAC__StreamDecoder* /*decoder*/, FLAC__StreamDecoderErrorStatus status,
                                void* clientData)
{
    auto* decoder = static_cast<FlacDecoder*>(clientData);
    if(!decoder) {
        return;
    }

    const QString error
        = u"FLAC decode error: %1"_s.arg(QString::fromLatin1(FLAC__StreamDecoderErrorStatusString[status]));
    if(decoder->m_options.testFlag(VerifyIntegrity)) {
        decoder->setError(error);
    }
    else if(!decoder->m_warnings.contains(error)) {
        decoder->m_warnings.push_back(error);
    }
}

void FlacDecoder::setError(QString error)
{
    if(m_error.isEmpty()) {
        m_error = std::move(error);
    }
}

uint64_t FlacDecoder::currentTimestamp() const
{
    if(m_format.sampleRate() <= 0) {
        return 0;
    }

    const auto rate = static_cast<uint64_t>(m_format.sampleRate());
    return ((m_currentFrame / rate) * 1000) + (((m_currentFrame % rate) * 1000) / rate);
}

bool FlacDecoder::finishStream()
{
    if(m_finished) {
        return m_error.isEmpty();
    }

    const bool complete = m_totalFrames == 0 || m_currentFrame == m_totalFrames;
    const bool verified = !m_options.testFlag(VerifyIntegrity) || FLAC__stream_decoder_finish(m_decoder.get());
    m_finished          = true;

    if(!complete) {
        setError(u"FLAC stream ended before all samples were decoded"_s);
    }
    else if(m_options.testFlag(VerifyIntegrity) && !verified) {
        setError(u"FLAC stream failed MD5 verification"_s);
    }

    return m_error.isEmpty();
}
} // namespace Fooyin::Flac
