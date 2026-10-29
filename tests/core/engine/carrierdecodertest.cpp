/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "core/engine/input/carrierdecoder.h"

#include <gtest/gtest.h>

#include <QFile>

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
class PcmDecoder final : public AudioDecoder
{
public:
    explicit PcmDecoder(QByteArray data)
        : m_data{std::move(data)}
    { }

    QStringList extensions() const override
    {
        return {u"pcm"_s};
    }

    bool isSeekable() const override
    {
        return true;
    }

    std::optional<AudioFormat> init(const AudioSource&, const Track&, DecoderOptions) override
    {
        m_offset = 0;
        return m_format;
    }

    void stop() override
    {
        m_offset = 0;
    }

    void seek(uint64_t pos) override
    {
        m_offset = std::min<qsizetype>(static_cast<qsizetype>(m_format.bytesForDuration(pos)), m_data.size());
    }

    ReadResult readAudio(size_t bytes) override
    {
        if(m_offset >= m_data.size()) {
            return ReadResult::endOfStream();
        }

        const qsizetype remaining = m_data.size() - m_offset;
        const qsizetype requested = std::min<qsizetype>(static_cast<qsizetype>(bytes), remaining);
        const qsizetype count     = requested - requested % m_format.bytesPerFrame();
        if(count == 0) {
            return ReadResult::endOfStream();
        }

        AudioBuffer buffer{reinterpret_cast<const uint8_t*>(m_data.constData() + m_offset), static_cast<size_t>(count),
                           m_format, m_format.durationForBytes(static_cast<uint64_t>(m_offset))};
        m_offset += count;
        return ReadResult::data(std::move(buffer));
    }

    AudioBuffer readBuffer(size_t bytes) override
    {
        auto result = readAudio(bytes);
        return result.status == ReadStatus::DecodedAudio ? std::move(result.buffer) : AudioBuffer{};
    }

private:
    QByteArray m_data;
    AudioFormat m_format{SampleFormat::S16, 44'100, 2};
    qsizetype m_offset{0};
};

QByteArray readFile(const QString& path)
{
    QFile file{path};
    if(!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QByteArray decodeAll(CarrierDecoder& decoder, const AudioFormat& format)
{
    QByteArray decoded;
    while(true) {
        auto result = decoder.readAudio(4096);
        if(result.status == AudioDecoder::ReadStatus::EndOfStream) {
            break;
        }
        EXPECT_EQ(result.status, AudioDecoder::ReadStatus::DecodedAudio);
        if(result.status != AudioDecoder::ReadStatus::DecodedAudio) {
            break;
        }
        EXPECT_EQ(result.buffer.format(), format);
        const auto data = result.buffer.constData();
        decoded.append(reinterpret_cast<const char*>(data.data()), static_cast<qsizetype>(data.size()));
    }
    return decoded;
}
} // namespace

TEST(CarrierDecoderTest, PreservesOrdinaryPcmAfterBoundedProbe)
{
    QByteArray pcm(320 * 1024, Qt::Uninitialized);
    for(qsizetype i{0}; i < pcm.size(); ++i) {
        pcm[i] = static_cast<char>((i * 31) & 0xff);
    }

    CarrierDecoder decoder{std::make_unique<PcmDecoder>(pcm)};
    const auto format = decoder.init({}, {}, AudioDecoder::None);
    ASSERT_TRUE(format);
    EXPECT_EQ(*format, AudioFormat(SampleFormat::S16, 44'100, 2));

    decoder.start();
    EXPECT_EQ(decodeAll(decoder, *format), pcm);
}

TEST(CarrierDecoderTest, DecodesContentDetectedEncodedAudio)
{
    QByteArray encoded = readFile(QString::fromUtf8(FOOYIN_TEST_SOURCE_DIR) + u"/data/audio/audiotest.mp3"_s);
    ASSERT_FALSE(encoded.isEmpty());
    encoded.append(QByteArray((4 - encoded.size() % 4) % 4, '\0'));

    CarrierDecoder decoder{std::make_unique<PcmDecoder>(encoded)};
    const auto format = decoder.init({}, {}, AudioDecoder::None);
    ASSERT_TRUE(format);
    EXPECT_EQ(format->sampleRate(), 48'000);
    EXPECT_EQ(format->channelCount(), 2);

    decoder.start();
    const auto result = decoder.readAudio(4096);
    ASSERT_EQ(result.status, AudioDecoder::ReadStatus::DecodedAudio);
    EXPECT_TRUE(result.buffer.isValid());
    EXPECT_EQ(result.buffer.format(), *format);
}
} // namespace Fooyin::Testing
