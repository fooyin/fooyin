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

#include "mpegdecoder.h"

#include <QLoggingCategory>

#include <cstdio>
#include <limits>

Q_LOGGING_CATEGORY(MPEG_DECODER, "fy.mpeg.decoder")

using namespace Qt::StringLiterals;

constexpr auto MaxResyncAttempts = 10;

namespace Fooyin::Mpg123 {
namespace {
bool initialiseMpg123()
{
    static const bool initialised = mpg123_init() == MPG123_OK;
    return initialised;
}
} // namespace

MpegDecoder::MpegDecoder()
    : m_decoder{nullptr}
    , m_device{nullptr}
    , m_currentFrame{0}
    , m_bitrate{0}
    , m_finished{false}
{ }

QStringList MpegDecoder::extensions() const
{
    return {u"mp1"_s, u"mp2"_s, u"mp3"_s};
}

bool MpegDecoder::isSeekable() const
{
    return m_decoder && m_device && !m_device->isSequential();
}

QStringList MpegDecoder::takeWarnings()
{
    return std::exchange(m_warnings, {});
}

int MpegDecoder::bitrate() const
{
    return m_bitrate;
}

std::optional<AudioFormat> MpegDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    m_device  = source.device;
    m_options = options;

    if(!m_device || !m_device->isOpen() || !initialiseMpg123()) {
        qCWarning(MPEG_DECODER) << "Unable to initialise MPEG audio decoder for" << track.filepath();
        return {};
    }

    int error{MPG123_OK};
    m_decoder.reset(mpg123_new(nullptr, &error));
    if(!m_decoder) {
        qCWarning(MPEG_DECODER) << "Unable to create MPEG audio decoder for" << track.filepath() << ":"
                                << mpg123_plain_strerror(error);
        return {};
    }

    if(mpg123_param(m_decoder.get(), MPG123_ADD_FLAGS, MPG123_GAPLESS | MPG123_QUIET, 0.0) != MPG123_OK
       || !configureOutput()
       || mpg123_replace_reader_handle(m_decoder.get(), readCallback, seekCallback, nullptr) != MPG123_OK
       || mpg123_open_handle(m_decoder.get(), this) != MPG123_OK || !updateFormat()) {
        qCWarning(MPEG_DECODER) << "Unable to open MPEG audio stream" << track.filepath() << ":" << decoderError();
        return {};
    }

    return m_format;
}

void MpegDecoder::stop()
{
    m_decoder.reset();
    m_device  = nullptr;
    m_format  = {};
    m_options = None;
    m_warnings.clear();
    m_currentFrame = 0;
    m_bitrate      = 0;
    m_finished     = false;
}

void MpegDecoder::seek(uint64_t pos)
{
    if(!isSeekable() || m_format.sampleRate() <= 0) {
        return;
    }

    const auto rate = static_cast<uint64_t>(m_format.sampleRate());
    uint64_t target = pos > std::numeric_limits<uint64_t>::max() / rate ? std::numeric_limits<uint64_t>::max()
                                                                        : (pos * rate) / 1000;
    target          = std::min(target, static_cast<uint64_t>(std::numeric_limits<off_t>::max()));

    const off_t result = mpg123_seek(m_decoder.get(), static_cast<off_t>(target), SEEK_SET);
    if(result < 0) {
        qCWarning(MPEG_DECODER) << "Failed to seek in MPEG audio stream:" << decoderError();
        return;
    }

    m_currentFrame = static_cast<uint64_t>(result);
    m_bitrate      = 0;
    m_finished     = false;
}

AudioDecoder::ReadResult MpegDecoder::readAudio(size_t bytes)
{
    if(!m_decoder || !m_format.isValid()) {
        return ReadResult::errorResult(u"MPEG audio decoder is not initialised"_s);
    }
    if(m_finished) {
        return ReadResult::endOfStream();
    }
    if(abortToken().stop_requested()) {
        return ReadResult::errorResult(u"MPEG audio decoding was cancelled"_s);
    }

    const auto bytesPerFrame = static_cast<size_t>(m_format.bytesPerFrame());
    const auto maxBytes      = static_cast<size_t>(std::numeric_limits<int>::max());
    const size_t requested   = std::min(bytes, maxBytes) - (std::min(bytes, maxBytes) % bytesPerFrame);
    if(requested == 0) {
        return ReadResult::needMoreInput();
    }

    AudioBuffer buffer{m_format, currentTimestamp()};
    buffer.resize(requested);

    int resyncAttempts{0};
    while(resyncAttempts < MaxResyncAttempts) {
        size_t decoded{0};
        int result = mpg123_read(m_decoder.get(), buffer.data(), requested, &decoded);
        if(result == MPG123_NEW_FORMAT) {
            if(!updateFormat()) {
                return ReadResult::errorResult(u"MPEG audio stream format changed unexpectedly"_s);
            }
            if(decoded == 0) {
                continue;
            }
            result = MPG123_OK;
        }

        if(decoded > requested || (decoded % bytesPerFrame) != 0) {
            return ReadResult::errorResult(u"MPEG audio decoder returned invalid audio data"_s);
        }

        if(decoded > 0) {
            buffer.resize(decoded);
            m_currentFrame += decoded / bytesPerFrame;
            m_finished = result == MPG123_DONE;
            updateBitrate();
            return ReadResult::data(std::move(buffer));
        }

        if(result == MPG123_DONE) {
            m_finished = true;
            return ReadResult::endOfStream();
        }
        if(result == MPG123_OK || result == MPG123_NEED_MORE) {
            return ReadResult::needMoreInput();
        }

        if(mpg123_errcode(m_decoder.get()) != MPG123_RESYNC_FAIL || m_options.testFlag(VerifyIntegrity)) {
            return ReadResult::errorResult(u"Failed to decode MPEG audio: %1"_s.arg(decoderError()));
        }

        ++resyncAttempts;
    }

    m_warnings.append(u"Ignored trailing junk after MPEG audio"_s);
    m_finished = true;
    return ReadResult::endOfStream();
}

AudioBuffer MpegDecoder::readBuffer(size_t bytes)
{
    auto result = readAudio(bytes);
    return result.status == ReadStatus::DecodedAudio ? std::move(result.buffer) : AudioBuffer{};
}

mpg123_ssize_t MpegDecoder::readCallback(void* handle, void* buffer, size_t bytes)
{
    auto* decoder = static_cast<MpegDecoder*>(handle);
    if(!decoder || !decoder->m_device || decoder->abortToken().stop_requested()) {
        return -1;
    }

    const auto maxRead = static_cast<size_t>(std::numeric_limits<qint64>::max());
    const qint64 read
        = decoder->m_device->read(static_cast<char*>(buffer), static_cast<qint64>(std::min(bytes, maxRead)));
    return static_cast<mpg123_ssize_t>(read);
}

off_t MpegDecoder::seekCallback(void* handle, off_t offset, int whence)
{
    auto* decoder = static_cast<MpegDecoder*>(handle);
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

    const qint64 target = base + relative;
    if(target < 0 || !std::in_range<off_t>(target) || !decoder->m_device->seek(target)) {
        return -1;
    }

    return static_cast<off_t>(target);
}

bool MpegDecoder::configureOutput() const
{
    if(mpg123_format_none(m_decoder.get()) != MPG123_OK) {
        return false;
    }

    const long* rates{nullptr};
    size_t rateCount{0};
    mpg123_rates(&rates, &rateCount);

    bool configured{false};
    for(size_t i{0}; i < rateCount; ++i) {
        configured
            |= mpg123_format(m_decoder.get(), rates[i], MPG123_MONO | MPG123_STEREO, MPG123_ENC_FLOAT_32) == MPG123_OK;
    }
    return configured;
}

bool MpegDecoder::updateFormat()
{
    long sampleRate{0};
    int channels{0};
    int encoding{0};
    if(mpg123_getformat(m_decoder.get(), &sampleRate, &channels, &encoding) != MPG123_OK) {
        return false;
    }

    const AudioFormat format{SampleFormat::F32, static_cast<int>(sampleRate), channels};
    if(m_format.isValid() && m_format != format) {
        return false;
    }

    m_format = format;
    return true;
}

void MpegDecoder::updateBitrate()
{
    mpg123_frameinfo info{};
    if(mpg123_info(m_decoder.get(), &info) != MPG123_OK) {
        return;
    }

    m_bitrate = info.vbr != MPG123_CBR && info.bitrate > 0 ? info.bitrate : 0;
}

QString MpegDecoder::decoderError() const
{
    return m_decoder ? QString::fromLocal8Bit(mpg123_strerror(m_decoder.get())) : u"Unknown error"_s;
}

uint64_t MpegDecoder::currentTimestamp() const
{
    if(m_format.sampleRate() <= 0) {
        return 0;
    }

    const auto rate = static_cast<uint64_t>(m_format.sampleRate());
    return ((m_currentFrame / rate) * 1000) + (((m_currentFrame % rate) * 1000) / rate);
}
} // namespace Fooyin::Mpg123
