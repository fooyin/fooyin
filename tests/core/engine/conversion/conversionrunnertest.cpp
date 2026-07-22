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
#include <core/engine/conversion/conversionrunner.h>
#include <core/engine/dsp/dspregistry.h>
#include <core/engine/input/ffmpeg/ffmpegencoder.h>
#include <core/engine/input/ffmpeg/ffmpeginput.h>
#include <core/engine/input/taglibparser.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <gtest/gtest.h>

extern "C"
{
#include <libavformat/avformat.h>
}

#include <cmath>
#include <cstring>
#include <optional>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
std::optional<AudioEncoderInfo> profileById(const std::vector<AudioEncoderInfo>& profiles, const QString& id)
{
    const auto it = std::ranges::find_if(profiles, [&id](const AudioEncoderInfo& info) { return info.id == id; });
    if(it == profiles.end()) {
        return {};
    }
    return *it;
}

bool hasCodec(const QString& path, AVCodecID codecId)
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
        if(params->codec_type == AVMEDIA_TYPE_AUDIO && params->codec_id == codecId) {
            return true;
        }
    }

    return false;
}

int audioChannelCount(const QString& path)
{
    AVFormatContext* context{nullptr};
    if(avformat_open_input(&context, path.toLocal8Bit().constData(), nullptr, nullptr) < 0) {
        return 0;
    }

    const auto close = qScopeGuard([&context] { avformat_close_input(&context); });
    if(avformat_find_stream_info(context, nullptr) < 0) {
        return 0;
    }
    for(unsigned i{0}; i < context->nb_streams; ++i) {
        const AVCodecParameters* params = context->streams[i]->codecpar;
        if(params->codec_type == AVMEDIA_TYPE_AUDIO) {
#if OLD_CHANNEL_LAYOUT
            return params->channels;
#else
            return params->ch_layout.nb_channels;
#endif
        }
    }
    return 0;
}

uint64_t audioDurationMs(const QString& path)
{
    AVFormatContext* context{nullptr};
    if(avformat_open_input(&context, path.toLocal8Bit().constData(), nullptr, nullptr) < 0) {
        return 0;
    }

    const auto close = qScopeGuard([&context] { avformat_close_input(&context); });
    if(avformat_find_stream_info(context, nullptr) < 0 || context->duration <= 0) {
        return 0;
    }
    return static_cast<uint64_t>(context->duration) * 1000 / AV_TIME_BASE;
}

QString outputTitle(const QString& path)
{
    AVFormatContext* context{nullptr};
    if(avformat_open_input(&context, path.toLocal8Bit().constData(), nullptr, nullptr) < 0) {
        return {};
    }

    const auto close = qScopeGuard([&context] { avformat_close_input(&context); });
    if(avformat_find_stream_info(context, nullptr) < 0) {
        return {};
    }

    const AVDictionaryEntry* title = av_dict_get(context->metadata, "title", nullptr, 0);
    return title ? QString::fromUtf8(title->value) : QString{};
}

double decodedPeak(AudioLoader& loader, const QString& path)
{
    auto loaded = loader.loadDecoderForTrack(Track{path}, AudioDecoder::NoLooping);
    if(!loaded.decoder || !loaded.format) {
        return 0.0;
    }

    loaded.decoder->start();
    const auto stop = qScopeGuard([&loaded] { loaded.decoder->stop(); });
    double peak{0.0};

    while(true) {
        auto result = loaded.decoder->readAudio(256 * 1024);
        if(result.status == AudioDecoder::ReadStatus::EndOfStream) {
            break;
        }
        if(result.status == AudioDecoder::ReadStatus::NeedMoreInput) {
            continue;
        }
        if(result.status != AudioDecoder::ReadStatus::DecodedAudio) {
            return 0.0;
        }

        AudioFormat format = result.buffer.format();
        format.setSampleFormat(SampleFormat::F64);

        const AudioBuffer converted = Audio::convert(result.buffer, format);
        if(!converted.isValid()) {
            return 0.0;
        }

        std::vector<double> samples(static_cast<size_t>(converted.sampleCount()));
        std::memcpy(samples.data(), converted.data(), static_cast<size_t>(converted.byteCount()));

        for(const double sample : samples) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    return peak;
}

class CapturingAudioEncoder : public AudioEncoder
{
public:
    explicit CapturingAudioEncoder(std::shared_ptr<std::vector<DitherMode>> ditherModes)
        : m_ditherModes{std::move(ditherModes)}
    { }

    [[nodiscard]] std::vector<AudioEncoderInfo> availableEncoders() const override
    {
        EncoderProfile profile;
        profile.id            = u"capture-wav"_s;
        profile.name          = u"Capture WAV"_s;
        profile.extension     = u"wav"_s;
        profile.containerName = u"wav"_s;
        profile.codecName     = u"capture"_s;
        return {{.id               = profile.id,
                 .backendId        = u"capture"_s,
                 .name             = profile.name,
                 .description      = {},
                 .profile          = profile,
                 .supportsMetadata = false,
                 .supportsPictures = false}};
    }

    Result init(const QString&, const AudioFormat&, const AudioEncoderSettings& settings) override
    {
        m_ditherModes->push_back(settings.ditherMode);
        return Result::success();
    }

    Result write(const AudioBuffer&) override
    {
        return Result::success();
    }

    Result finish() override
    {
        return Result::success();
    }

private:
    std::shared_ptr<std::vector<DitherMode>> m_ditherModes;
};

class CountingDsp : public DspNode
{
public:
    [[nodiscard]] QString name() const override
    {
        return u"Counting DSP"_s;
    }

    [[nodiscard]] QString id() const override
    {
        return u"test.dsp.counting"_s;
    }

    void prepare(const AudioFormat&) override { }
    void process(ProcessingBufferList&) override { }
};
} // namespace

TEST(ConversionRunnerTest, ConvertsTrackToFlac)
{
    const auto flacProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-flac"_s);
    if(!flacProfile) {
        GTEST_SKIP() << "Runtime FFmpeg FLAC encoder is unavailable";
    }

    QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });
    loader.addReader(u"TagLib"_s, [] { return std::make_unique<TagLibReader>(); });

    AudioEncoderRegistry registry;
    registry.addEncoderBackend(u"ffmpeg"_s, u"FFmpeg"_s, [] { return std::make_unique<FFmpegEncoder>(); });

    Track track{QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s)};
    track.setTitle(u"Converted Track"_s);
    track.setRating(0.8F);
    track.setPlayCount(12);

    ConversionPreset preset;
    preset.encoder.profile              = flacProfile->profile;
    preset.encoder.outputSampleFormat   = SampleFormat::S16;
    preset.destination.mode             = DestinationMode::FixedFolder;
    preset.destination.fixedFolder      = outputDir.path();
    preset.destination.filenamePattern  = u"%title%"_s;
    preset.destination.existingFileMode = ExistingFileMode::Overwrite;
    preset.destination.outputStyle      = OutputStyle::IndividualFiles;
    preset.processing.transferPlaycount = true;
    preset.other.verifyOutput           = true;

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &registry;
    request.job.tracks      = {track};
    request.job.preset      = preset;

    const auto results = ConversionRunner::run(request);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results.front().status, ConversionResultStatus::Succeeded) << results.front().error.toStdString();
    EXPECT_TRUE(QFileInfo::exists(results.front().outputPath));
    EXPECT_GT(QFileInfo{results.front().outputPath}.size(), 0);
    EXPECT_TRUE(hasCodec(results.front().outputPath, AV_CODEC_ID_FLAC));
    EXPECT_EQ(outputTitle(results.front().outputPath), u"Converted Track"_s);

    Track convertedTrack{results.front().outputPath};
    ASSERT_TRUE(loader.readTrackMetadata(convertedTrack));
    EXPECT_FLOAT_EQ(convertedTrack.rating(), 0.8F);
    EXPECT_EQ(convertedTrack.playCount(), 12);

    bool existingFileCallbackCalled{false};
    request.job.preset.destination.existingFileMode = ExistingFileMode::Ask;
    request.existingFileCallback                    = [&existingFileCallbackCalled](const QString&) {
        existingFileCallbackCalled = true;
        return ExistingFileMode::Skip;
    };

    const auto skippedResults = ConversionRunner::run(request);

    ASSERT_EQ(skippedResults.size(), 1);
    EXPECT_TRUE(existingFileCallbackCalled);
    EXPECT_EQ(skippedResults.front().status, ConversionResultStatus::Skipped);
    EXPECT_EQ(outputTitle(skippedResults.front().outputPath), u"Converted Track"_s);

    request.job.preset.processing.transferMetadata = false;
    request.existingFileCallback                   = [](const QString&) {
        return ExistingFileMode::Overwrite;
    };

    const auto resultsWithoutMetadata = ConversionRunner::run(request);

    ASSERT_EQ(resultsWithoutMetadata.size(), 1);
    EXPECT_EQ(resultsWithoutMetadata.front().status, ConversionResultStatus::Succeeded)
        << resultsWithoutMetadata.front().error.toStdString();
    EXPECT_TRUE(outputTitle(resultsWithoutMetadata.front().outputPath).isEmpty());

    Track convertedWithoutMetadata{resultsWithoutMetadata.front().outputPath};
    ASSERT_TRUE(loader.readTrackMetadata(convertedWithoutMetadata));
    EXPECT_FLOAT_EQ(convertedWithoutMetadata.rating(), 0.8F);
    EXPECT_EQ(convertedWithoutMetadata.playCount(), 12);
}

TEST(ConversionRunnerTest, ReportsFailedDecode)
{
    QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });
    auto ditherModes = std::make_shared<std::vector<DitherMode>>();
    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"capture"_s, u"Capture"_s,
                                      [ditherModes] { return std::make_unique<CapturingAudioEncoder>(ditherModes); });

    Track track{outputDir.filePath(u"missing.wav"_s)};
    track.setTitle(u"Missing"_s);
    ConversionPreset preset;
    preset.encoder.profile.id            = u"capture-wav"_s;
    preset.encoder.profile.extension     = u"wav"_s;
    preset.encoder.profile.containerName = u"wav"_s;
    preset.encoder.profile.codecName     = u"capture"_s;
    preset.destination.mode              = DestinationMode::FixedFolder;
    preset.destination.fixedFolder       = outputDir.path();
    preset.destination.filenamePattern   = u"%title%"_s;
    preset.destination.existingFileMode  = ExistingFileMode::Overwrite;

    const QByteArray originalContents{"existing output"};
    QFile existingOutput{outputDir.filePath(u"Missing.wav"_s)};
    ASSERT_TRUE(existingOutput.open(QIODevice::WriteOnly));
    ASSERT_EQ(existingOutput.write(originalContents), originalContents.size());
    existingOutput.close();

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.job.tracks      = {track};
    request.job.preset      = preset;

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results.front().status, ConversionResultStatus::Failed);
    EXPECT_FALSE(results.front().error.isEmpty());
    EXPECT_TRUE(ditherModes->empty());
    ASSERT_TRUE(existingOutput.open(QIODevice::ReadOnly));
    EXPECT_EQ(existingOutput.readAll(), originalContents);
}

TEST(ConversionRunnerTest, CancelsAtTrackBoundary)
{
    QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());
    QFile existing{outputDir.filePath(u"First.wav"_s)};
    ASSERT_TRUE(existing.open(QIODevice::WriteOnly));
    existing.close();

    AudioLoader loader;
    auto ditherModes = std::make_shared<std::vector<DitherMode>>();
    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"capture"_s, u"Capture"_s,
                                      [ditherModes] { return std::make_unique<CapturingAudioEncoder>(ditherModes); });

    Track first{outputDir.filePath(u"first-source.wav"_s)};
    first.setTitle(u"First"_s);
    Track second{outputDir.filePath(u"second-source.wav"_s)};
    second.setTitle(u"Second"_s);
    ConversionPreset preset;
    preset.encoder.profile.id            = u"capture-wav"_s;
    preset.encoder.profile.extension     = u"wav"_s;
    preset.encoder.profile.containerName = u"wav"_s;
    preset.encoder.profile.codecName     = u"capture"_s;
    preset.destination.mode              = DestinationMode::FixedFolder;
    preset.destination.fixedFolder       = outputDir.path();
    preset.destination.filenamePattern   = u"%title%"_s;
    preset.destination.existingFileMode  = ExistingFileMode::Skip;

    int cancellationChecks{0};
    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.job.tracks      = {first, second};
    request.job.preset      = preset;
    request.cancelCallback  = [&cancellationChecks] {
        return cancellationChecks++ > 0;
    };

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].status, ConversionResultStatus::Skipped);
    EXPECT_EQ(results[1].status, ConversionResultStatus::Cancelled);
    EXPECT_TRUE(ditherModes->empty());
}

TEST(ConversionRunnerTest, AppliesTrackReplayGainBeforeEncoding)
{
    const auto wavProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-wav"_s);
    if(!wavProfile) {
        GTEST_SKIP() << "Runtime FFmpeg WAV encoder is unavailable";
    }

    QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });

    AudioEncoderRegistry registry;
    registry.addEncoderBackend(u"ffmpeg"_s, u"FFmpeg"_s, [] { return std::make_unique<FFmpegEncoder>(); });

    Track track{QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s)};
    track.setTitle(u"ReplayGain Off"_s);
    track.setRGTrackGain(-6.0F);

    ConversionPreset preset;
    preset.encoder.profile              = wavProfile->profile;
    preset.encoder.outputSampleFormat   = SampleFormat::S16;
    preset.destination.mode             = DestinationMode::FixedFolder;
    preset.destination.fixedFolder      = outputDir.path();
    preset.destination.filenamePattern  = u"%title%"_s;
    preset.destination.existingFileMode = ExistingFileMode::Overwrite;

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &registry;
    request.job.tracks      = {track};
    request.job.preset      = preset;

    const auto unprocessedResults = ConversionRunner::run(request);
    ASSERT_EQ(unprocessedResults.size(), 1);
    ASSERT_EQ(unprocessedResults.front().status, ConversionResultStatus::Succeeded)
        << unprocessedResults.front().error.toStdString();
    const double unprocessedPeak = decodedPeak(loader, unprocessedResults.front().outputPath);
    ASSERT_GT(unprocessedPeak, 0.0);

    track.setTitle(u"ReplayGain Track"_s);
    request.job.tracks                                      = {track};
    request.job.preset.processing.replayGainMode            = ConversionReplayGainMode::Track;
    request.job.preset.processing.replayGainPreventClipping = false;

    const auto processedResults = ConversionRunner::run(request);
    ASSERT_EQ(processedResults.size(), 1);
    ASSERT_EQ(processedResults.front().status, ConversionResultStatus::Succeeded)
        << processedResults.front().error.toStdString();
    const double processedPeak = decodedPeak(loader, processedResults.front().outputPath);
    ASSERT_GT(processedPeak, 0.0);

    EXPECT_NEAR(processedPeak / unprocessedPeak, std::pow(10.0, -6.0 / 20.0), 0.02);
}

TEST(ConversionRunnerTest, AppliesIndependentDspChainBeforeEncoding)
{
    const auto wavProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-wav"_s);
    if(!wavProfile) {
        GTEST_SKIP() << "Runtime FFmpeg WAV encoder is unavailable";
    }

    QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });

    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"ffmpeg"_s, u"FFmpeg"_s, [] { return std::make_unique<FFmpegEncoder>(); });
    DspRegistry dspRegistry;

    Track track{QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s)};
    track.setTitle(u"DSP Mono"_s);

    ConversionPreset preset;
    preset.encoder.profile              = wavProfile->profile;
    preset.encoder.outputSampleFormat   = SampleFormat::S16;
    preset.destination.mode             = DestinationMode::FixedFolder;
    preset.destination.fixedFolder      = outputDir.path();
    preset.destination.filenamePattern  = u"%title%"_s;
    preset.destination.existingFileMode = ExistingFileMode::Overwrite;
    preset.processing.dspChain          = {{.id          = u"fooyin.dsp.downmixtomono"_s,
                                            .name        = u"Downmix to mono"_s,
                                            .hasSettings = false,
                                            .enabled     = true,
                                            .instanceId  = 1,
                                            .settings    = {}}};

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.dspRegistry     = &dspRegistry;
    request.job.tracks      = {track};
    request.job.preset      = preset;

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results.front().status, ConversionResultStatus::Succeeded) << results.front().error.toStdString();
    EXPECT_EQ(audioChannelCount(results.front().outputPath), 1);
}

TEST(ConversionRunnerTest, ResolvesLossySourceDitherPerTrack)
{
    QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });

    auto ditherModes = std::make_shared<std::vector<DitherMode>>();
    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"capture"_s, u"Capture"_s,
                                      [ditherModes] { return std::make_unique<CapturingAudioEncoder>(ditherModes); });

    const QString sourcePath
        = QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s);
    Track lossyTrack{sourcePath};
    lossyTrack.setTitle(u"Lossy Source"_s);
    lossyTrack.setEncoding(u"Lossy"_s);
    Track losslessTrack{sourcePath};
    losslessTrack.setTitle(u"Lossless Source"_s);
    losslessTrack.setEncoding(u"Lossless"_s);

    ConversionPreset preset;
    preset.encoder.profile.id            = u"capture-wav"_s;
    preset.encoder.profile.name          = u"Capture WAV"_s;
    preset.encoder.profile.extension     = u"wav"_s;
    preset.encoder.profile.containerName = u"wav"_s;
    preset.encoder.profile.codecName     = u"capture"_s;
    preset.encoder.outputSampleFormat    = SampleFormat::S16;
    preset.encoder.ditherMode            = DitherMode::LossySourceOnly;
    preset.destination.mode              = DestinationMode::FixedFolder;
    preset.destination.fixedFolder       = outputDir.path();
    preset.destination.filenamePattern   = u"%title%"_s;
    preset.destination.existingFileMode  = ExistingFileMode::Overwrite;

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.job.tracks      = {lossyTrack, losslessTrack};
    request.job.preset      = preset;

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].status, ConversionResultStatus::Succeeded);
    EXPECT_EQ(results[1].status, ConversionResultStatus::Succeeded);
    ASSERT_EQ(ditherModes->size(), 2);
    EXPECT_EQ((*ditherModes)[0], DitherMode::Always);
    EXPECT_EQ((*ditherModes)[1], DitherMode::Never);
}

TEST(ConversionRunnerTest, GeneratesPercentagePreview)
{
    const auto wavProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-wav"_s);
    if(!wavProfile) {
        GTEST_SKIP() << "Runtime FFmpeg WAV encoder is unavailable";
    }

    QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });

    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"ffmpeg"_s, u"FFmpeg"_s, [] { return std::make_unique<FFmpegEncoder>(); });

    Track track{QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s)};
    track.setTitle(u"Preview Output"_s);
    track.setDuration(1835);

    ConversionPreset preset;
    preset.encoder.profile              = wavProfile->profile;
    preset.encoder.outputSampleFormat   = SampleFormat::S16;
    preset.destination.mode             = DestinationMode::FixedFolder;
    preset.destination.fixedFolder      = outputDir.path();
    preset.destination.filenamePattern  = u"%title%"_s;
    preset.destination.existingFileMode = ExistingFileMode::Overwrite;
    preset.preview.enabled              = true;
    preset.preview.lengthPercentage     = 20;

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.job.tracks      = {track};
    request.job.preset      = preset;

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results.front().status, ConversionResultStatus::Succeeded) << results.front().error.toStdString();
    EXPECT_NEAR(static_cast<double>(audioDurationMs(results.front().outputPath)), 367.0, 5.0);

    track.setTitle(u"Unknown Duration"_s);
    track.setDuration(0);
    request.job.tracks       = {track};
    const auto failedResults = ConversionRunner::run(request);
    ASSERT_EQ(failedResults.size(), 1);
    EXPECT_EQ(failedResults.front().status, ConversionResultStatus::Failed);
    EXPECT_TRUE(failedResults.front().error.contains(u"duration"_s, Qt::CaseInsensitive));
}

TEST(ConversionRunnerTest, CopiesMatchingSidecarFiles)
{
    const auto wavProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-wav"_s);
    if(!wavProfile) {
        GTEST_SKIP() << "Runtime FFmpeg WAV encoder is unavailable";
    }

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QDir root{tempDir.path()};
    ASSERT_TRUE(root.mkpath(u"source"_s));
    ASSERT_TRUE(root.mkpath(u"output"_s));

    const QString fixture = QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s);
    const QString sourcePath = root.filePath(u"source/input.wav"_s);
    ASSERT_TRUE(QFile::copy(fixture, sourcePath));

    QFile cover{root.filePath(u"source/cover.jpg"_s)};
    ASSERT_TRUE(cover.open(QIODevice::WriteOnly));
    ASSERT_EQ(cover.write("cover"), 5);
    cover.close();
    QFile notes{root.filePath(u"source/notes.txt"_s)};
    ASSERT_TRUE(notes.open(QIODevice::WriteOnly));
    ASSERT_EQ(notes.write("notes"), 5);
    notes.close();

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });
    loader.addReader(u"TagLib"_s, [] { return std::make_unique<TagLibReader>(); });
    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"ffmpeg"_s, u"FFmpeg"_s, [] { return std::make_unique<FFmpegEncoder>(); });

    Track track{sourcePath};
    track.setTitle(u"Converted"_s);
    ConversionPreset preset;
    preset.encoder.profile              = wavProfile->profile;
    preset.encoder.outputSampleFormat   = SampleFormat::S16;
    preset.destination.mode             = DestinationMode::FixedFolder;
    preset.destination.fixedFolder      = root.filePath(u"output"_s);
    preset.destination.filenamePattern  = u"%title%"_s;
    preset.destination.existingFileMode = ExistingFileMode::Overwrite;
    preset.other.copyFilesPattern       = u"*.jpg; *.log"_s;
    preset.other.verifyOutput           = true;

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.job.tracks      = {track};
    request.job.preset      = preset;

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results.front().status, ConversionResultStatus::Succeeded) << results.front().error.toStdString();
    EXPECT_TRUE(QFileInfo::exists(root.filePath(u"output/cover.jpg"_s)));
    EXPECT_FALSE(QFileInfo::exists(root.filePath(u"output/notes.txt"_s)));
    EXPECT_TRUE(results.front().warnings.isEmpty());
}

TEST(ConversionRunnerTest, GeneratesGroupedMultiTrackOutputs)
{
    const auto flacProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-flac"_s);
    if(!flacProfile) {
        GTEST_SKIP() << "Runtime FFmpeg FLAC encoder is unavailable";
    }

    QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });
    loader.addReader(u"TagLib"_s, [] { return std::make_unique<TagLibReader>(); });

    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"ffmpeg"_s, u"FFmpeg"_s, [] { return std::make_unique<FFmpegEncoder>(); });

    const QString sourcePath
        = QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s);
    Track first{sourcePath};
    first.setTitle(u"First Track"_s);
    first.setAlbum(u"Album A"_s);
    first.setRating(1.0F);
    first.setPlayCount(42);
    Track second{sourcePath};
    second.setTitle(u"Second Track"_s);
    second.setAlbum(u"Album A"_s);
    Track third{sourcePath};
    third.setTitle(u"Third Track"_s);
    third.setAlbum(u"Album B"_s);

    ConversionPreset preset;
    preset.encoder.profile              = flacProfile->profile;
    preset.encoder.outputSampleFormat   = SampleFormat::S16;
    preset.destination.mode             = DestinationMode::FixedFolder;
    preset.destination.fixedFolder      = outputDir.path();
    preset.destination.filenamePattern  = u"%album%"_s;
    preset.destination.existingFileMode = ExistingFileMode::Overwrite;
    preset.destination.outputStyle      = OutputStyle::MultiTrackFiles;
    preset.processing.transferPlaycount = true;

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.job.tracks      = {first, second, third};
    request.job.preset      = preset;

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 3);
    for(const auto& result : results) {
        ASSERT_EQ(result.status, ConversionResultStatus::Succeeded) << result.error.toStdString();
    }

    EXPECT_EQ(results[0].outputPath, results[1].outputPath);
    EXPECT_EQ(QFileInfo{results[0].outputPath}.fileName(), u"Album A.flac"_s);
    EXPECT_EQ(QFileInfo{results[2].outputPath}.fileName(), u"Album B.flac"_s);
    EXPECT_NE(results[0].outputPath, results[2].outputPath);
    EXPECT_NEAR(static_cast<double>(audioDurationMs(results[0].outputPath)), 3669.0, 5.0);
    EXPECT_NEAR(static_cast<double>(audioDurationMs(results[2].outputPath)), 1834.0, 5.0);
    EXPECT_EQ(outputTitle(results[0].outputPath), u"First Track"_s);
    EXPECT_EQ(outputTitle(results[2].outputPath), u"Third Track"_s);

    Track groupedTrack{results[0].outputPath};
    ASSERT_TRUE(loader.readTrackMetadata(groupedTrack));
    EXPECT_LT(groupedTrack.rating(), 0.0F);
    EXPECT_EQ(groupedTrack.playCount(), 0);
}

TEST(ConversionRunnerTest, PreservesDspStateAcrossCombinedOutputWhenRequested)
{
    const auto wavProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-wav"_s);
    if(!wavProfile) {
        GTEST_SKIP() << "Runtime FFmpeg WAV encoder is unavailable";
    }

    QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });

    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"ffmpeg"_s, u"FFmpeg"_s, [] { return std::make_unique<FFmpegEncoder>(); });

    auto factoryCount = std::make_shared<int>(0);
    DspRegistry dspRegistry;
    dspRegistry.registerDsp({.id = u"test.dsp.counting"_s, .name = u"Counting DSP"_s, .factory = [factoryCount] {
                                 ++(*factoryCount);
                                 return std::make_unique<CountingDsp>();
                             }});

    const QString sourcePath
        = QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s);
    Track first{sourcePath};
    first.setTitle(u"Counting DSP"_s);
    Track second{sourcePath};
    second.setTitle(u"Second Track"_s);

    ConversionPreset preset;
    preset.encoder.profile              = wavProfile->profile;
    preset.encoder.outputSampleFormat   = SampleFormat::S16;
    preset.destination.mode             = DestinationMode::FixedFolder;
    preset.destination.fixedFolder      = outputDir.path();
    preset.destination.filenamePattern  = u"%title%"_s;
    preset.destination.existingFileMode = ExistingFileMode::Overwrite;
    preset.destination.outputStyle      = OutputStyle::MergeTracks;
    preset.processing.dspChain          = {{.id          = u"test.dsp.counting"_s,
                                            .name        = u"Counting DSP"_s,
                                            .hasSettings = false,
                                            .enabled     = true,
                                            .instanceId  = 1,
                                            .settings    = {}}};

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.dspRegistry     = &dspRegistry;
    request.job.tracks      = {first, second};
    request.job.preset      = preset;

    const auto resetResults = ConversionRunner::run(request);
    ASSERT_EQ(resetResults.size(), 2);
    ASSERT_EQ(resetResults[0].status, ConversionResultStatus::Succeeded) << resetResults[0].error.toStdString();
    ASSERT_EQ(resetResults[1].status, ConversionResultStatus::Succeeded) << resetResults[1].error.toStdString();
    EXPECT_EQ(*factoryCount, 2);

    *factoryCount                                               = 0;
    request.job.preset.processing.preserveDspStateBetweenTracks = true;
    const auto preservedResults                                 = ConversionRunner::run(request);
    ASSERT_EQ(preservedResults.size(), 2);
    ASSERT_EQ(preservedResults[0].status, ConversionResultStatus::Succeeded) << preservedResults[0].error.toStdString();
    ASSERT_EQ(preservedResults[1].status, ConversionResultStatus::Succeeded) << preservedResults[1].error.toStdString();
    EXPECT_EQ(*factoryCount, 1);
}
} // namespace Fooyin::Testing
