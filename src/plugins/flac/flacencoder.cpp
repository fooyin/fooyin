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

#include "flacencoder.h"

#include <core/engine/audioconverter.h>

#include <FLAC/format.h>

#include <QFileInfo>

#include <cstring>
#include <limits>
#include <vector>

using namespace Qt::StringLiterals;

constexpr auto DefaultCompressionLevel = 5;

namespace Fooyin::Flac {
namespace {
AudioFormat::ChannelLayout flacChannelLayout(int channels)
{
    using P = AudioFormat::ChannelPosition;
    if(channels == 7) {
        return {P::FrontLeft, P::FrontRight, P::FrontCenter, P::LFE, P::BackCenter, P::SideLeft, P::SideRight};
    }
    return AudioFormat::defaultChannelLayoutForChannelCount(channels);
}

bool hasCompatibleChannelLayout(const AudioFormat& input, const AudioFormat::ChannelLayout& outputLayout)
{
    if(!input.hasChannelLayout()) {
        return true;
    }

    const auto inputLayout = input.channelLayoutView();
    return std::ranges::all_of(outputLayout, [inputLayout](AudioFormat::ChannelPosition position) {
        return std::ranges::find(inputLayout, position) != inputLayout.end();
    });
}

SampleFormat automaticSampleFormat(SampleFormat input)
{
    switch(input) {
        case SampleFormat::U8:
        case SampleFormat::S16:
        case SampleFormat::S24In32:
            return input;
        case SampleFormat::S32:
            return FLAC__MAX_BITS_PER_SAMPLE >= 32 ? SampleFormat::S32 : SampleFormat::S24In32;
        case SampleFormat::F32:
        case SampleFormat::F64:
        case SampleFormat::Unknown:
        default:
            return SampleFormat::S24In32;
    }
}

int bitsPerSample(SampleFormat format)
{
    switch(format) {
        case SampleFormat::U8:
            return 8;
        case SampleFormat::S16:
            return 16;
        case SampleFormat::S24In32:
            return 24;
        case SampleFormat::S32:
            return 32;
        case SampleFormat::F32:
        case SampleFormat::F64:
        case SampleFormat::Unknown:
        default:
            return 0;
    }
}

bool shouldDither(DitherMode mode, SampleFormat input, SampleFormat output)
{
    if(mode == DitherMode::Always) {
        return true;
    }
    if(mode != DitherMode::Automatic) {
        return false;
    }
    if(input == SampleFormat::F32 || input == SampleFormat::F64) {
        return true;
    }

    const AudioFormat inputFormat{input, 1, 1};
    const AudioFormat outputFormat{output, 1, 1};
    return inputFormat.isValid() && outputFormat.isValid()
        && outputFormat.bitsPerSample() < inputFormat.bitsPerSample();
}

QString encoderStateError(const QString& prefix, const FLAC__StreamEncoder* encoder)
{
    const auto state = FLAC__stream_encoder_get_state(encoder);
    return prefix + u": "_s + QString::fromLatin1(FLAC__StreamEncoderStateString[state]);
}
} // namespace

FlacEncoder::FlacEncoder()
    : m_encoder{nullptr}
    , m_dither{false}
    , m_initialised{false}
    , m_finished{false}
{ }

FlacEncoder::~FlacEncoder()
{
    cleanup();
}

std::vector<AudioEncoderInfo> FlacEncoder::availableEncoders() const
{
    EncoderProfile profile;
    profile.id               = u"flac"_s;
    profile.name             = u"FLAC"_s;
    profile.extension        = u"flac"_s;
    profile.containerName    = u"flac"_s;
    profile.codecName        = u"flac"_s;
    profile.mode             = EncoderMode::LosslessCompression;
    profile.compressionLevel = DefaultCompressionLevel;

    AudioEncoderInfo info;
    info.id                            = profile.id;
    info.backendId                     = u"flac"_s;
    info.name                          = profile.name;
    info.description                   = u"FLAC, level %1"_s.arg(profile.compressionLevel);
    info.profile                       = std::move(profile);
    info.capabilities.modes            = {EncoderMode::LosslessCompression};
    info.capabilities.compressionLevel = {.minimum = 0, .maximum = 8, .step = 1};
    info.supportsMetadata              = true;
    info.supportsPictures              = true;

    return {std::move(info)};
}

AudioEncoder::Result FlacEncoder::init(const QString& outputPath, const AudioFormat& input,
                                       const AudioEncoderSettings& settings)
{
    cleanup();

    if(outputPath.isEmpty()) {
        return Result::failure(u"Output path is empty"_s);
    }
    if(!input.isValid()) {
        return Result::failure(u"Input format is invalid"_s);
    }
    if(input.channelCount() < 1 || std::cmp_greater(input.channelCount(), FLAC__MAX_CHANNELS)) {
        return Result::failure(u"FLAC supports between 1 and %1 channels"_s.arg(FLAC__MAX_CHANNELS));
    }
    if(!FLAC__format_sample_rate_is_valid(static_cast<uint32_t>(input.sampleRate()))) {
        return Result::failure(u"The input sample rate is not supported by FLAC"_s);
    }

    const auto outputLayout = flacChannelLayout(input.channelCount());
    if(!hasCompatibleChannelLayout(input, outputLayout)) {
        return Result::failure(u"The input channel layout cannot be represented by FLAC"_s);
    }

    const SampleFormat outputSampleFormat = settings.outputSampleFormat == SampleFormat::Unknown
                                              ? automaticSampleFormat(input.sampleFormat())
                                              : settings.outputSampleFormat;
    const int outputBits                  = bitsPerSample(outputSampleFormat);
    if(outputBits == 0 || std::cmp_greater(outputBits, FLAC__MAX_BITS_PER_SAMPLE)) {
        return Result::failure(u"Requested output sample format is unsupported by the FLAC encoder"_s);
    }

    FLAC__StreamEncoder* encoder = FLAC__stream_encoder_new();
    if(!encoder) {
        return Result::failure(u"Failed to allocate FLAC encoder"_s);
    }
    m_encoder.reset(encoder);

    m_inputFormat  = input;
    m_outputFormat = input;
    m_outputFormat.setSampleFormat(outputSampleFormat);
    m_outputFormat.setChannelLayout(outputLayout);
    m_dither = shouldDither(settings.ditherMode, input.sampleFormat(), outputSampleFormat);

    const int compressionLevel = settings.profile.compressionLevel >= 0
                                   ? std::clamp(settings.profile.compressionLevel, 0, 8)
                                   : DefaultCompressionLevel;
    const bool configured
        = FLAC__stream_encoder_set_channels(encoder, static_cast<uint32_t>(input.channelCount()))
       && FLAC__stream_encoder_set_bits_per_sample(encoder, static_cast<uint32_t>(outputBits))
       && FLAC__stream_encoder_set_sample_rate(encoder, static_cast<uint32_t>(input.sampleRate()))
       && FLAC__stream_encoder_set_compression_level(encoder, static_cast<uint32_t>(compressionLevel));
    if(!configured) {
        const QString error = encoderStateError(u"Failed to configure FLAC encoder"_s, encoder);
        cleanup();
        return Result::failure(error);
    }

    m_file.setFileName(outputPath);
    if(!m_file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        const QString error = u"Failed to open output file: %1"_s.arg(m_file.errorString());
        cleanup();
        return Result::failure(error);
    }

    const auto status
        = FLAC__stream_encoder_init_stream(encoder, writeCallback, seekCallback, tellCallback, nullptr, this);
    if(status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        const QString error = u"Failed to initialise FLAC encoder: %1"_s.arg(
            QString::fromLatin1(FLAC__StreamEncoderInitStatusString[status]));
        cleanup();
        return Result::failure(error);
    }

    m_initialised = true;
    m_finished    = false;
    return Result::success();
}

AudioEncoder::Result FlacEncoder::write(const AudioBuffer& buffer)
{
    if(!m_initialised || m_finished || !m_encoder) {
        return Result::failure(u"Encoder is not initialised"_s);
    }
    if(!buffer.isValid()) {
        return Result::failure(u"Audio buffer is invalid"_s);
    }
    if(buffer.format() != m_inputFormat) {
        return Result::failure(u"Audio buffer format changed during encoding"_s);
    }
    if(!m_error.isEmpty()) {
        return Result::failure(m_error);
    }

    AudioBuffer converted
        = buffer.format() == m_outputFormat ? buffer : Audio::convert(buffer, m_outputFormat, m_dither);
    if(!converted.isValid()) {
        return Result::failure(u"Failed to convert audio for the FLAC encoder"_s);
    }

    std::vector<FLAC__int32> samples(static_cast<size_t>(converted.sampleCount()));
    const std::byte* source = converted.data();

    switch(m_outputFormat.sampleFormat()) {
        case SampleFormat::U8:
            for(size_t i{0}; i < samples.size(); ++i) {
                samples[i] = static_cast<FLAC__int32>(std::to_integer<uint8_t>(source[i])) - 128;
            }
            break;
        case SampleFormat::S16:
            for(size_t i{0}; i < samples.size(); ++i) {
                int16_t value;
                std::memcpy(&value, source + (i * sizeof(value)), sizeof(value));
                samples[i] = value;
            }
            break;
        case SampleFormat::S24In32:
            for(size_t i{0}; i < samples.size(); ++i) {
                int32_t value;
                std::memcpy(&value, source + (i * sizeof(value)), sizeof(value));
                samples[i] = value >> 8;
            }
            break;
        case SampleFormat::S32:
            for(size_t i{0}; i < samples.size(); ++i) {
                std::memcpy(&samples[i], source + (i * sizeof(FLAC__int32)), sizeof(FLAC__int32));
            }
            break;
        case SampleFormat::F32:
        case SampleFormat::F64:
        case SampleFormat::Unknown:
        default:
            return Result::failure(u"Unsupported FLAC sample format"_s);
    }

    if(!FLAC__stream_encoder_process_interleaved(m_encoder.get(), samples.data(),
                                                 static_cast<uint32_t>(converted.frameCount()))) {
        const QString error
            = !m_error.isEmpty() ? m_error : encoderStateError(u"Failed to encode FLAC audio"_s, m_encoder.get());
        return Result::failure(error);
    }

    return Result::success();
}

AudioEncoder::Result FlacEncoder::finish()
{
    if(m_finished || !m_encoder) {
        return Result::success();
    }

    const bool finished = !m_initialised || FLAC__stream_encoder_finish(m_encoder.get());
    m_initialised       = false;
    m_finished          = true;
    m_file.close();

    QString error{m_error};
    if(!finished && error.isEmpty()) {
        error = encoderStateError(u"Failed to finish FLAC stream"_s, m_encoder.get());
    }

    m_encoder.reset();

    return error.isEmpty() ? Result::success() : Result::failure(error);
}

FLAC__StreamEncoderWriteStatus FlacEncoder::writeCallback(const FLAC__StreamEncoder* /*encoder*/,
                                                          const FLAC__byte buffer[], size_t bytes, uint32_t /*samples*/,
                                                          uint32_t /*currentFrame*/, void* clientData)
{
    auto* encoder = static_cast<FlacEncoder*>(clientData);
    if(!encoder) {
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    }

    size_t written{0};
    while(written < bytes) {
        const qint64 result = encoder->m_file.write(reinterpret_cast<const char*>(buffer + written),
                                                    static_cast<qint64>(bytes - written));
        if(result <= 0) {
            encoder->setError(u"Failed to write FLAC output: %1"_s.arg(encoder->m_file.errorString()));
            return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
        }
        written += static_cast<size_t>(result);
    }

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLAC__StreamEncoderSeekStatus FlacEncoder::seekCallback(const FLAC__StreamEncoder* /*encoder*/, FLAC__uint64 offset,
                                                        void* clientData)
{
    auto* encoder = static_cast<FlacEncoder*>(clientData);
    if(!encoder || offset > static_cast<FLAC__uint64>(std::numeric_limits<qint64>::max())) {
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
    }

    return encoder->m_file.seek(static_cast<qint64>(offset)) ? FLAC__STREAM_ENCODER_SEEK_STATUS_OK
                                                             : FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
}

FLAC__StreamEncoderTellStatus FlacEncoder::tellCallback(const FLAC__StreamEncoder* /*encoder*/, FLAC__uint64* offset,
                                                        void* clientData)
{
    const auto* encoder = static_cast<const FlacEncoder*>(clientData);
    if(!encoder || encoder->m_file.pos() < 0) {
        return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
    }

    *offset = static_cast<FLAC__uint64>(encoder->m_file.pos());
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

void FlacEncoder::cleanup()
{
    if(m_encoder && m_initialised) {
        FLAC__stream_encoder_finish(m_encoder.get());
    }
    m_encoder.reset();

    m_file.close();
    m_file.setFileName({});

    m_inputFormat  = {};
    m_outputFormat = {};
    m_error.clear();
    m_dither      = false;
    m_initialised = false;
    m_finished    = false;
}

void FlacEncoder::setError(QString error)
{
    if(m_error.isEmpty()) {
        m_error = std::move(error);
    }
}
} // namespace Fooyin::Flac
