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

#include <core/engine/input/ffmpeg/ffmpegencoder.h>

#include <gtest/gtest.h>

#include <QDir>
#include <QFileInfo>
#include <QScopeGuard>
#include <QTemporaryDir>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {

namespace {

bool hasEncoder(const QStringList& codecNames)
{
    for(const QString& codecName : codecNames) {
        if(avcodec_find_encoder_by_name(codecName.toUtf8().constData())) {
            return true;
        }
    }
    return false;
}

bool hasMuxer(const QString& containerName, const QString& extension)
{
    return av_guess_format(containerName.toUtf8().constData(), nullptr, extension.toUtf8().constData()) != nullptr;
}

bool hasProfile(const std::vector<AudioEncoderInfo>& profiles, const QString& id)
{
    return std::ranges::any_of(profiles, [&id](const AudioEncoderInfo& info) { return info.id == id; });
}

std::optional<AudioEncoderInfo> profileById(const std::vector<AudioEncoderInfo>& profiles, const QString& id)
{
    const auto it = std::ranges::find_if(profiles, [&id](const AudioEncoderInfo& info) { return info.id == id; });
    if(it == profiles.end()) {
        return {};
    }
    return *it;
}

AudioBuffer makeSineBuffer()
{
    constexpr int SampleRate = 44100;
    constexpr int Channels   = 2;
    constexpr int Frames     = SampleRate / 10;

    AudioFormat format{SampleFormat::S16, SampleRate, Channels};
    std::vector<int16_t> samples(Frames * Channels);
    for(int frame{0}; frame < Frames; ++frame) {
        const double phase = (static_cast<double>(frame) * 440.0 * 2.0 * 3.14159265358979323846) / SampleRate;
        const auto sample  = static_cast<int16_t>(std::sin(phase) * 12000.0);
        samples[static_cast<size_t>(frame * Channels)]     = sample;
        samples[static_cast<size_t>(frame * Channels + 1)] = sample;
    }

    return {reinterpret_cast<const uint8_t*>(samples.data()), samples.size() * sizeof(int16_t), format, 0};
}

bool outputHasAudioStream(const QString& path, AVCodecID expectedCodec, int expectedSampleRate,
                          int expectedBitsPerSample = 0)
{
    AVFormatContext* context{nullptr};
    if(avformat_open_input(&context, path.toLocal8Bit().constData(), nullptr, nullptr) < 0) {
        return false;
    }

    const auto close = qScopeGuard([&context] { avformat_close_input(&context); });

    if(avformat_find_stream_info(context, nullptr) < 0) {
        return false;
    }

    for(unsigned i{0}; i < context->nb_streams; ++i) {
        const AVCodecParameters* params = context->streams[i]->codecpar;
        if(params->codec_type != AVMEDIA_TYPE_AUDIO || params->codec_id != expectedCodec) {
            continue;
        }
        if(expectedBitsPerSample > 0
           && std::max(params->bits_per_raw_sample, params->bits_per_coded_sample) != expectedBitsPerSample) {
            return false;
        }
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100)
        return params->sample_rate == expectedSampleRate && params->channels == 2;
#else
        return params->sample_rate == expectedSampleRate && params->ch_layout.nb_channels == 2;
#endif
    }

    return false;
}

void encodeSmokeTest(const QString& profileId, AVCodecID expectedCodec, int bitrateKbps = 0, int compressionLevel = -1,
                     int expectedSampleRate = 44100, SampleFormat sampleFormat = SampleFormat::S16,
                     int expectedBitsPerSample = 0, EncoderMode mode = EncoderMode::Default, double quality = 2.0)
{
    const auto profile = profileById(FFmpegEncoder{}.availableEncoders(), profileId);
    if(!profile) {
        GTEST_SKIP() << "Runtime FFmpeg profile is unavailable";
    }

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString outputPath = QDir{dir.path()}.filePath(u"encoded.%1"_s.arg(profile->profile.extension));
    AudioEncoderSettings settings;
    settings.profile            = profile->profile;
    settings.outputSampleFormat = sampleFormat;
    if(bitrateKbps > 0) {
        settings.profile.bitrateKbps = bitrateKbps;
    }
    if(compressionLevel >= 0) {
        settings.profile.compressionLevel = compressionLevel;
    }
    if(mode != EncoderMode::Default) {
        settings.profile.mode = mode;
    }
    settings.profile.quality = quality;

    FFmpegEncoder encoder;
    auto result = encoder.init(outputPath, makeSineBuffer().format(), settings);
    ASSERT_TRUE(result.ok) << result.error.toStdString();

    result = encoder.write(makeSineBuffer());
    ASSERT_TRUE(result.ok) << result.error.toStdString();

    result = encoder.finish();
    ASSERT_TRUE(result.ok) << result.error.toStdString();

    EXPECT_TRUE(QFileInfo::exists(outputPath));
    EXPECT_GT(QFileInfo{outputPath}.size(), 0);
    EXPECT_TRUE(outputHasAudioStream(outputPath, expectedCodec, expectedSampleRate, expectedBitsPerSample));
}

} // namespace

TEST(FFmpegEncoderTest, ListsOnlyProfilesWithRuntimeEncoderAndMuxer)
{
    const auto profiles = FFmpegEncoder{}.availableEncoders();

    EXPECT_EQ(hasProfile(profiles, u"ffmpeg-wav"_s), hasEncoder({u"pcm_s16le"_s}) && hasMuxer(u"wav"_s, u"wav"_s));
    EXPECT_EQ(hasProfile(profiles, u"ffmpeg-flac"_s), hasEncoder({u"flac"_s}) && hasMuxer(u"flac"_s, u"flac"_s));
    EXPECT_EQ(hasProfile(profiles, u"ffmpeg-alac"_s), hasEncoder({u"alac"_s}) && hasMuxer(u"ipod"_s, u"m4a"_s));
    EXPECT_EQ(hasProfile(profiles, u"ffmpeg-wavpack"_s), hasEncoder({u"wavpack"_s}) && hasMuxer(u"wv"_s, u"wv"_s));
    EXPECT_EQ(hasProfile(profiles, u"ffmpeg-mp3"_s),
              hasEncoder({u"libmp3lame"_s, u"mp3"_s}) && hasMuxer(u"mp3"_s, u"mp3"_s));
    EXPECT_EQ(hasProfile(profiles, u"ffmpeg-aac"_s), hasEncoder({u"aac"_s}) && hasMuxer(u"ipod"_s, u"m4a"_s));
    EXPECT_EQ(hasProfile(profiles, u"ffmpeg-vorbis"_s), hasEncoder({u"libvorbis"_s}) && hasMuxer(u"ogg"_s, u"ogg"_s));
    EXPECT_EQ(hasProfile(profiles, u"ffmpeg-opus"_s),
              hasEncoder({u"libopus"_s, u"opus"_s}) && hasMuxer(u"ogg"_s, u"opus"_s));
}

TEST(FFmpegEncoderTest, ProfileIdsAreUniqueAndComplete)
{
    const auto profiles = FFmpegEncoder{}.availableEncoders();
    std::set<QString> ids;

    for(const AudioEncoderInfo& info : profiles) {
        EXPECT_FALSE(info.id.isEmpty());
        EXPECT_FALSE(info.backendId.isEmpty());
        EXPECT_FALSE(info.name.isEmpty());
        EXPECT_FALSE(info.profile.id.isEmpty());
        EXPECT_FALSE(info.profile.extension.isEmpty());
        EXPECT_FALSE(info.profile.containerName.isEmpty());
        EXPECT_FALSE(info.profile.codecName.isEmpty());
        EXPECT_TRUE(ids.insert(info.id).second);
    }
}

TEST(FFmpegEncoderTest, Mp3AdvertisesGenericRateControlCapabilities)
{
    const auto profile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-mp3"_s);
    if(!profile) {
        GTEST_SKIP() << "Runtime FFmpeg MP3 encoder is unavailable";
    }

    EXPECT_EQ(profile->capabilities.modes,
              (std::vector{EncoderMode::ConstantQuality, EncoderMode::AverageBitrate, EncoderMode::ConstantBitrate}));
    EXPECT_TRUE(profile->capabilities.quality.isValid());
    EXPECT_TRUE(profile->capabilities.bitrateKbps.isValid());
    EXPECT_TRUE(profile->capabilities.compressionLevel.isValid());
    ASSERT_TRUE(profile->estimateBitrateKbps);

    EncoderProfile encoderProfile = profile->profile;
    encoderProfile.quality        = 0.0;
    EXPECT_EQ(profile->estimateBitrateKbps(encoderProfile), 245);
    encoderProfile.quality = 2.0;
    EXPECT_EQ(profile->estimateBitrateKbps(encoderProfile), 190);
    encoderProfile.quality = 9.0;
    EXPECT_EQ(profile->estimateBitrateKbps(encoderProfile), 65);
}

TEST(FFmpegEncoderTest, EncodesWav)
{
    encodeSmokeTest(u"ffmpeg-wav"_s, AV_CODEC_ID_PCM_S16LE);
}

TEST(FFmpegEncoderTest, EncodesFlac)
{
    encodeSmokeTest(u"ffmpeg-flac"_s, AV_CODEC_ID_FLAC, 0, 12);
}

TEST(FFmpegEncoderTest, EncodesAlac)
{
    encodeSmokeTest(u"ffmpeg-alac"_s, AV_CODEC_ID_ALAC);
}

TEST(FFmpegEncoderTest, EncodesWavPack)
{
    encodeSmokeTest(u"ffmpeg-wavpack"_s, AV_CODEC_ID_WAVPACK, 0, 8);
}

TEST(FFmpegEncoderTest, Encodes24BitWav)
{
    if(!hasEncoder({u"pcm_s24le"_s})) {
        GTEST_SKIP() << "Runtime FFmpeg 24-bit PCM encoder is unavailable";
    }
    encodeSmokeTest(u"ffmpeg-wav"_s, AV_CODEC_ID_PCM_S24LE, 0, -1, 44100, SampleFormat::S24In32, 24);
}

TEST(FFmpegEncoderTest, EncodesRequestedWavSampleFormats)
{
    struct FormatCase
    {
        QString codecName;
        AVCodecID codecId;
        SampleFormat sampleFormat;
        int bitsPerSample;
    };
    const std::array cases{
        FormatCase{u"pcm_u8"_s, AV_CODEC_ID_PCM_U8, SampleFormat::U8, 8},
        FormatCase{u"pcm_s32le"_s, AV_CODEC_ID_PCM_S32LE, SampleFormat::S32, 32},
        FormatCase{u"pcm_f32le"_s, AV_CODEC_ID_PCM_F32LE, SampleFormat::F32, 32},
    };

    for(const auto& testCase : cases) {
        SCOPED_TRACE(testCase.codecName.toStdString());
        if(!hasEncoder({testCase.codecName})) {
            continue;
        }
        encodeSmokeTest(u"ffmpeg-wav"_s, testCase.codecId, 0, -1, 44100, testCase.sampleFormat, testCase.bitsPerSample);
    }
}

TEST(FFmpegEncoderTest, Encodes24BitFlac)
{
    encodeSmokeTest(u"ffmpeg-flac"_s, AV_CODEC_ID_FLAC, 0, 5, 44100, SampleFormat::S24In32, 24);
}

TEST(FFmpegEncoderTest, RejectsUnsupportedExplicitSampleFormat)
{
    const auto profile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-flac"_s);
    if(!profile) {
        GTEST_SKIP() << "Runtime FFmpeg FLAC encoder is unavailable";
    }

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    AudioEncoderSettings settings;
    settings.profile            = profile->profile;
    settings.outputSampleFormat = SampleFormat::F32;

    FFmpegEncoder encoder;
    const auto result
        = encoder.init(QDir{dir.path()}.filePath(u"unsupported.flac"_s), makeSineBuffer().format(), settings);
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.error.contains(u"unsupported"_s, Qt::CaseInsensitive));
}

TEST(FFmpegEncoderTest, EncodesMp3InSupportedRateControlModes)
{
    encodeSmokeTest(u"ffmpeg-mp3"_s, AV_CODEC_ID_MP3, 224, 2, 44100, SampleFormat::S16, 0,
                    EncoderMode::ConstantBitrate);
    encodeSmokeTest(u"ffmpeg-mp3"_s, AV_CODEC_ID_MP3, 192, 2, 44100, SampleFormat::S16, 0, EncoderMode::AverageBitrate);
    encodeSmokeTest(u"ffmpeg-mp3"_s, AV_CODEC_ID_MP3, 0, 2, 44100, SampleFormat::S16, 0, EncoderMode::ConstantQuality,
                    2.0);
}

TEST(FFmpegEncoderTest, EncodesAacInSupportedRateControlModes)
{
    const auto profile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-aac"_s);
    if(!profile) {
        GTEST_SKIP() << "Runtime FFmpeg AAC encoder is unavailable";
    }

    encodeSmokeTest(u"ffmpeg-aac"_s, AV_CODEC_ID_AAC, 256, -1, 44100, SampleFormat::Unknown, 0,
                    EncoderMode::ConstantBitrate);
    encodeSmokeTest(u"ffmpeg-aac"_s, AV_CODEC_ID_AAC, 0, -1, 44100, SampleFormat::Unknown, 0,
                    EncoderMode::ConstantQuality, 2.0);
}

TEST(FFmpegEncoderTest, EncodesVorbis)
{
    encodeSmokeTest(u"ffmpeg-vorbis"_s, AV_CODEC_ID_VORBIS, 0, -1, 44100, SampleFormat::Unknown, 0,
                    EncoderMode::ConstantQuality, 5.0);
}

TEST(FFmpegEncoderTest, EncodesOpusInAdvertisedBitrateModes)
{
    const auto profile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-opus"_s);
    if(!profile) {
        GTEST_SKIP() << "Runtime FFmpeg Opus encoder is unavailable";
    }

    for(const EncoderMode mode : profile->capabilities.modes) {
        SCOPED_TRACE(static_cast<int>(mode));
        encodeSmokeTest(u"ffmpeg-opus"_s, AV_CODEC_ID_OPUS, 160, -1, 48000, SampleFormat::S16, 0, mode);
    }
}

} // namespace Fooyin::Testing
