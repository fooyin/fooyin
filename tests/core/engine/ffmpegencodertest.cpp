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

#include <core/engine/audioconverter.h>
#include <core/engine/input/ffmpeg/ffmpegencoder.h>
#include <core/engine/input/ffmpeg/ffmpeginput.h>

#include <gtest/gtest.h>

#include <QBuffer>
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
#include <cstring>
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

void appendSynchsafe32(QByteArray& data, quint32 value)
{
    data.append(static_cast<char>((value >> 21U) & 0x7FU));
    data.append(static_cast<char>((value >> 14U) & 0x7FU));
    data.append(static_cast<char>((value >> 7U) & 0x7FU));
    data.append(static_cast<char>(value & 0x7FU));
}

QByteArray id3TextFrame(QByteArrayView id, QByteArrayView value)
{
    QByteArray payload;
    payload.append(char{3});
    payload.append(value);

    QByteArray frame{id.data(), id.size()};
    const quint32 size = static_cast<quint32>(payload.size());
    frame.append(static_cast<char>((size >> 24U) & 0xFFU));
    frame.append(static_cast<char>((size >> 16U) & 0xFFU));
    frame.append(static_cast<char>((size >> 8U) & 0xFFU));
    frame.append(static_cast<char>(size & 0xFFU));
    frame.append(QByteArray{2, '\0'});
    frame.append(payload);
    return frame;
}

QByteArray timedId3Tag()
{
    QByteArray frames;
    frames.append(id3TextFrame("TIT2", "Test Title"));
    frames.append(id3TextFrame("TPE1", "Test Artist"));

    QByteArray tag{"ID3\x03\x00\x00", 6};
    appendSynchsafe32(tag, static_cast<quint32>(frames.size()));
    tag.append(frames);
    return tag;
}

QByteArray makeTimedId3TransportStream()
{
    AVFormatContext* context{nullptr};
    if(avformat_alloc_output_context2(&context, nullptr, "mpegts", nullptr) < 0 || !context) {
        return {};
    }
    const auto freeContext = qScopeGuard([&context] { avformat_free_context(context); });

    AVStream* audio = avformat_new_stream(context, nullptr);
    AVStream* id3   = avformat_new_stream(context, nullptr);
    if(!audio || !id3) {
        return {};
    }

    audio->time_base             = {1, 44'100};
    audio->codecpar->codec_type  = AVMEDIA_TYPE_AUDIO;
    audio->codecpar->codec_id    = AV_CODEC_ID_AAC;
    audio->codecpar->sample_rate = 44'100;
    audio->codecpar->bit_rate    = 64'000;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100)
    audio->codecpar->channels       = 2;
    audio->codecpar->channel_layout = AV_CH_LAYOUT_STEREO;
#else
    av_channel_layout_default(&audio->codecpar->ch_layout, 2);
#endif
    audio->codecpar->extradata = static_cast<uint8_t*>(av_mallocz(2 + AV_INPUT_BUFFER_PADDING_SIZE));
    if(!audio->codecpar->extradata) {
        return {};
    }
    audio->codecpar->extradata_size = 2;
    audio->codecpar->extradata[0]   = 0x12;
    audio->codecpar->extradata[1]   = 0x10;

    id3->time_base            = {1, 1000};
    id3->codecpar->codec_type = AVMEDIA_TYPE_DATA;
    id3->codecpar->codec_id   = AV_CODEC_ID_TIMED_ID3;

    if(avio_open_dyn_buf(&context->pb) < 0 || avformat_write_header(context, nullptr) < 0) {
        return {};
    }

    const QByteArray tag = timedId3Tag();
    AVPacket* packet     = av_packet_alloc();
    if(!packet) {
        return {};
    }
    const auto freePacket = qScopeGuard([&packet] { av_packet_free(&packet); });
    if(av_new_packet(packet, tag.size()) < 0) {
        return {};
    }
    std::memcpy(packet->data, tag.constData(), static_cast<size_t>(tag.size()));
    packet->stream_index = id3->index;
    packet->pts          = 1234;
    packet->dts          = 1234;
    packet->flags        = AV_PKT_FLAG_KEY;
    if(av_interleaved_write_frame(context, packet) < 0 || av_write_trailer(context) < 0) {
        return {};
    }

    uint8_t* output{nullptr};
    const int outputSize = avio_close_dyn_buf(context->pb, &output);
    context->pb          = nullptr;
    if(outputSize <= 0 || !output) {
        av_free(output);
        return {};
    }

    QByteArray result{reinterpret_cast<const char*>(output), outputSize};
    av_free(output);
    return result;
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

AudioBuffer makeSineBuffer(SampleFormat sampleFormat = SampleFormat::S16)
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

    AudioBuffer buffer{reinterpret_cast<const uint8_t*>(samples.data()), samples.size() * sizeof(int16_t), format, 0};
    if(sampleFormat == SampleFormat::S16) {
        return buffer;
    }

    format.setSampleFormat(sampleFormat);
    return Audio::convert(buffer, format);
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
                     int expectedBitsPerSample = 0, EncoderMode mode = EncoderMode::Default, double quality = 2.0,
                     SampleFormat inputSampleFormat = SampleFormat::S16)
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

    const AudioBuffer input = makeSineBuffer(inputSampleFormat);
    FFmpegEncoder encoder;
    auto result = encoder.init(outputPath, input.format(), settings);
    ASSERT_TRUE(result.ok) << result.error.toStdString();

    result = encoder.write(input);
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
    EXPECT_EQ(profile->profile.mode, EncoderMode::ConstantQuality);
    EXPECT_DOUBLE_EQ(profile->profile.quality, 2.0);
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

TEST(FFmpegEncoderTest, AutomaticSampleFormatPreserves24BitWav)
{
    if(!hasEncoder({u"pcm_s24le"_s})) {
        GTEST_SKIP() << "Runtime FFmpeg 24-bit PCM encoder is unavailable";
    }
    encodeSmokeTest(u"ffmpeg-wav"_s, AV_CODEC_ID_PCM_S24LE, 0, -1, 44100, SampleFormat::Unknown, 24,
                    EncoderMode::Default, 2.0, SampleFormat::S24In32);
}

TEST(FFmpegEncoderTest, AutomaticSampleFormatPreserves24BitFlac)
{
    encodeSmokeTest(u"ffmpeg-flac"_s, AV_CODEC_ID_FLAC, 0, 5, 44100, SampleFormat::Unknown, 24, EncoderMode::Default,
                    2.0, SampleFormat::S24In32);
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

TEST(FFmpegInputTest, ReadsTimedId3FromMpegTsDataStream)
{
    QByteArray transportStream = makeTimedId3TransportStream();
    ASSERT_FALSE(transportStream.isEmpty());

    QBuffer input{&transportStream};
    ASSERT_TRUE(input.open(QIODevice::ReadOnly));

    const Track track{u"timed-id3.ts"_s};
    const AudioSource source{.filepath = track.filepath(), .device = &input};
    FFmpegDecoder decoder;
    ASSERT_TRUE(decoder.init(source, track, AudioDecoder::UpdateTracks).has_value());
    decoder.start();

    static_cast<void>(decoder.readAudio(4096));
    const auto change = decoder.takeTimedTrackChange();

    ASSERT_TRUE(change.has_value());
    EXPECT_GT(change->timestampMs, 0);
    EXPECT_EQ(change->track.title(), u"Test Title"_s);
    EXPECT_EQ(change->track.artist(), u"Test Artist"_s);
}

} // namespace Fooyin::Testing
