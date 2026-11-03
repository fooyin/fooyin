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
#include <utils/scopeguard.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <gtest/gtest.h>

extern "C"
{
#include <libavformat/avformat.h>
}

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <thread>
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

    const auto close = scopeGuard([&context] { avformat_close_input(&context); });
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

    const auto close = scopeGuard([&context] { avformat_close_input(&context); });
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

    const auto close = scopeGuard([&context] { avformat_close_input(&context); });
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

    const auto close = scopeGuard([&context] { avformat_close_input(&context); });
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
    const auto stop = scopeGuard([&loaded] { loaded.decoder->stop(); });
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
        return {{.id                  = profile.id,
                 .backendId           = u"capture"_s,
                 .backendName         = u"Capture"_s,
                 .name                = profile.name,
                 .description         = {},
                 .profile             = profile,
                 .estimateBitrateKbps = {},
                 .supportsMetadata    = false,
                 .supportsPictures    = false}};
    }

    Result init(const QString& /*outputPath*/, const AudioFormat& /*inputFormat*/,
                const AudioEncoderSettings& settings) override
    {
        m_ditherModes->push_back(settings.ditherMode);
        return Result::success();
    }

    Result write(const AudioBuffer& /*buffer*/) override
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

struct WarningDecoderState
{
    AudioDecoder::DecoderOptions options{AudioDecoder::None};
    int warningDrains{0};
};

class WarningDecoder : public AudioDecoder
{
public:
    explicit WarningDecoder(std::shared_ptr<WarningDecoderState> state)
        : m_state{std::move(state)}
    { }

    [[nodiscard]] QStringList extensions() const override
    {
        return {u"warning"_s};
    }

    [[nodiscard]] bool isSeekable() const override
    {
        return true;
    }

    std::optional<AudioFormat> init(const AudioSource& /*source*/, const Track& /*track*/,
                                    DecoderOptions options) override
    {
        m_state->options = options;
        m_emitted        = false;
        m_warningPending = true;
        return m_format;
    }

    void seek(uint64_t /*pos*/) override { }
    void stop() override { }

    AudioBuffer readBuffer(size_t bytes) override
    {
        if(m_emitted || std::cmp_less(bytes, m_format.bytesPerFrame())) {
            return {};
        }
        m_emitted = true;
        AudioBuffer buffer{m_format, 0};
        buffer.resize(100 * static_cast<size_t>(m_format.bytesPerFrame()));
        buffer.fillSilence();
        return buffer;
    }

    QStringList takeWarnings() override
    {
        ++m_state->warningDrains;
        return std::exchange(m_warningPending, false) ? QStringList{u"Recovered source read"_s} : QStringList{};
    }

private:
    std::shared_ptr<WarningDecoderState> m_state;
    AudioFormat m_format{SampleFormat::S16, 44100, 2};
    bool m_emitted{false};
    bool m_warningPending{false};
};

struct ParallelDecoderState
{
    std::mutex mutex;
    std::condition_variable ready;
    int active{0};
    int peakActive{0};
    bool waitForPeers{true};
    std::vector<std::pair<QString, QString>> startedTracks;
};

class ParallelDecoder : public FFmpegDecoder
{
public:
    explicit ParallelDecoder(std::shared_ptr<ParallelDecoderState> state)
        : m_state{std::move(state)}
    { }

    ~ParallelDecoder() override
    {
        if(m_opened) {
            const std::scoped_lock lock{m_state->mutex};
            --m_state->active;
        }
    }

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override
    {
        const auto format = FFmpegDecoder::init(source, track, options);
        if(format) {
            std::unique_lock lock{m_state->mutex};
            m_state->startedTracks.emplace_back(track.album(), track.title());
            m_opened            = true;
            m_state->peakActive = std::max(m_state->peakActive, ++m_state->active);
            m_state->ready.notify_all();
            if(m_state->waitForPeers && m_state->peakActive < 2) {
                m_state->ready.wait_for(lock, std::chrono::seconds{2}, [this] { return m_state->peakActive >= 2; });
            }
        }
        return format;
    }

private:
    std::shared_ptr<ParallelDecoderState> m_state;
    bool m_opened{false};
};

class ThreadCheckingObserver : public ConversionInputObserver
{
public:
    explicit ThreadCheckingObserver(std::thread::id expectedThread)
        : tracksStarted{0}
        , wrongThread{false}
        , m_expectedThread{expectedThread}
    { }

    void trackStarted(const Track& /*track*/, const AudioFormat& /*format*/) override
    {
        checkThread();
        ++tracksStarted;
    }

    void sourceAudio(const Track& /*track*/, const AudioBuffer& /*buffer*/) override
    {
        checkThread();
    }

    void trackFinished(const Track& /*track*/, bool /*complete*/) override
    {
        checkThread();
    }

    std::atomic_int tracksStarted;
    std::atomic_bool wrongThread;

private:
    void checkThread()
    {
        if(std::this_thread::get_id() != m_expectedThread) {
            wrongThread.store(true);
        }
    }

    std::thread::id m_expectedThread;
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

    void prepare(const AudioFormat& /*format*/) override { }
    void process(ProcessingBufferList& /*chunks*/) override { }
};
} // namespace

TEST(ConversionRunnerTest, ConvertsTrackToFlac)
{
    const auto flacProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-flac"_s);
    if(!flacProfile) {
        GTEST_SKIP() << "Runtime FFmpeg FLAC encoder is unavailable";
    }

    const QTemporaryDir outputDir;
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

TEST(ConversionRunnerTest, BoundsParallelConversionsAndPreservesResultOrder)
{
    const auto profile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-flac"_s);
    if(!profile) {
        GTEST_SKIP() << "Runtime FFmpeg FLAC encoder is unavailable";
    }

    const QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    const auto state = std::make_shared<ParallelDecoderState>();
    AudioLoader loader;
    loader.addDecoder(u"Parallel"_s, [state] { return std::make_unique<ParallelDecoder>(state); });
    loader.addReader(u"TagLib"_s, [] { return std::make_unique<TagLibReader>(); });
    AudioEncoderRegistry registry;
    registry.addEncoderBackend(u"ffmpeg"_s, u"FFmpeg"_s, [] { return std::make_unique<FFmpegEncoder>(); });

    ConversionRunner::Request request;
    request.audioLoader                             = &loader;
    request.encoderRegistry                         = &registry;
    request.maxParallelConversions                  = 2;
    request.job.preset.encoder.profile              = profile->profile;
    request.job.preset.destination.mode             = DestinationMode::FixedFolder;
    request.job.preset.destination.fixedFolder      = outputDir.path();
    request.job.preset.destination.filenamePattern  = u"%title%"_s;
    request.job.preset.destination.existingFileMode = ExistingFileMode::Overwrite;
    request.job.preset.other.verifyOutput           = true;
    const QString fixture = QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s);
    for(const QString& title : {u"First"_s, u"Second"_s, u"Third"_s}) {
        Track track{fixture};
        track.setTitle(title);
        request.job.tracks.push_back(std::move(track));
    }

    std::vector<bool> finished(request.job.tracks.size());
    request.progressCallback = [&](const ConversionProgress& progress) {
        ASSERT_GE(progress.trackIndex, 0);
        const auto i = static_cast<size_t>(progress.trackIndex);
        ASSERT_LT(i, finished.size());
        EXPECT_FALSE(finished[i]);
        if(progress.finished) {
            finished[i] = true;
            EXPECT_TRUE(QFileInfo::exists(progress.outputPath));
        }
    };

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(state->peakActive, 2);
    EXPECT_EQ(state->active, 0);
    for(size_t i{0}; i < results.size(); ++i) {
        EXPECT_TRUE(finished[i]);
        ASSERT_EQ(results[i].status, ConversionResultStatus::Succeeded) << results[i].error.toStdString();
        EXPECT_EQ(results[i].sourceTrack.title(), request.job.tracks[i].title());
        EXPECT_EQ(outputTitle(results[i].outputPath), request.job.tracks[i].title());
        EXPECT_NEAR(static_cast<double>(audioDurationMs(results[i].outputPath)), 1834.0, 5.0);
    }

    const auto observer      = std::make_shared<ThreadCheckingObserver>(std::this_thread::get_id());
    request.sourceObserver   = observer;
    request.progressCallback = {};
    state->waitForPeers      = false;
    state->peakActive        = 0;

    const auto observedResults = ConversionRunner::run(request);
    ASSERT_EQ(observedResults.size(), 3);
    for(const auto& result : observedResults) {
        EXPECT_EQ(result.status, ConversionResultStatus::Succeeded) << result.error.toStdString();
    }
    EXPECT_EQ(state->peakActive, 1);
    EXPECT_EQ(observer->tracksStarted.load(), 3);
    EXPECT_FALSE(observer->wrongThread.load());
}

TEST(ConversionRunnerTest, ReportsFailedDecode)
{
    const QTemporaryDir outputDir;
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

TEST(ConversionRunnerTest, MarksSourceAsConversionAndForwardsDecoderWarnings)
{
    const QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());
    const QString sourcePath = outputDir.filePath(u"source.warning"_s);
    QFile source{sourcePath};
    ASSERT_TRUE(source.open(QIODevice::WriteOnly));
    source.close();

    auto decoderState = std::make_shared<WarningDecoderState>();
    AudioLoader loader;
    loader.addDecoder(u"Warning"_s, [decoderState] { return std::make_unique<WarningDecoder>(decoderState); });

    auto ditherModes = std::make_shared<std::vector<DitherMode>>();
    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"capture"_s, u"Capture"_s,
                                      [ditherModes] { return std::make_unique<CapturingAudioEncoder>(ditherModes); });

    Track track{sourcePath};
    track.setTitle(u"Warning Source"_s);
    ConversionPreset preset;
    preset.encoder.profile.id            = u"capture-wav"_s;
    preset.encoder.profile.extension     = u"wav"_s;
    preset.encoder.profile.containerName = u"wav"_s;
    preset.encoder.profile.codecName     = u"capture"_s;
    preset.destination.mode              = DestinationMode::FixedFolder;
    preset.destination.fixedFolder       = outputDir.path();
    preset.destination.filenamePattern   = u"%title%"_s;
    preset.destination.existingFileMode  = ExistingFileMode::Overwrite;
    preset.processing.transferMetadata   = false;

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.job.tracks      = {track};
    request.job.preset      = preset;

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(1, results.size());
    ASSERT_EQ(ConversionResultStatus::Succeeded, results.front().status) << results.front().error.toStdString();
    EXPECT_TRUE(decoderState->options.testFlag(AudioDecoder::ForConversion));
    EXPECT_TRUE(decoderState->options.testFlag(AudioDecoder::NoLooping));
    EXPECT_EQ(1, results.front().warnings.count(u"Recovered source read"_s));
    EXPECT_EQ(1, decoderState->warningDrains);
}

TEST(ConversionRunnerTest, CancelsAtTrackBoundary)
{
    const QTemporaryDir outputDir;
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

    const QTemporaryDir outputDir;
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

    const QTemporaryDir outputDir;
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
    const QTemporaryDir outputDir;
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

TEST(ConversionRunnerTest, EnablesAutomaticDitherWhenReducingBitDepth)
{
    const QTemporaryDir outputDir;
    ASSERT_TRUE(outputDir.isValid());

    AudioLoader loader;
    loader.addDecoder(u"FFmpeg"_s, [] { return std::make_unique<FFmpegDecoder>(); });

    auto ditherModes = std::make_shared<std::vector<DitherMode>>();
    AudioEncoderRegistry encoderRegistry;
    encoderRegistry.addEncoderBackend(u"capture"_s, u"Capture"_s,
                                      [ditherModes] { return std::make_unique<CapturingAudioEncoder>(ditherModes); });

    const QString sourcePath
        = QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s);
    Track track{sourcePath};
    track.setTitle(u"Automatic Dither"_s);

    ConversionPreset preset;
    preset.encoder.profile.id            = u"capture-wav"_s;
    preset.encoder.profile.extension     = u"wav"_s;
    preset.encoder.profile.containerName = u"wav"_s;
    preset.encoder.profile.codecName     = u"capture"_s;
    preset.encoder.outputSampleFormat    = SampleFormat::S16;
    preset.encoder.ditherMode            = DitherMode::Automatic;
    preset.destination.mode              = DestinationMode::FixedFolder;
    preset.destination.fixedFolder       = outputDir.path();
    preset.destination.filenamePattern   = u"%title%"_s;
    preset.destination.existingFileMode  = ExistingFileMode::Overwrite;
    preset.processing.replayGainMode     = ConversionReplayGainMode::Track;

    ConversionRunner::Request request;
    request.audioLoader     = &loader;
    request.encoderRegistry = &encoderRegistry;
    request.job.tracks      = {track};
    request.job.preset      = preset;

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results.front().status, ConversionResultStatus::Succeeded);
    ASSERT_EQ(ditherModes->size(), 1);
    EXPECT_EQ(ditherModes->front(), DitherMode::Always);
}

TEST(ConversionRunnerTest, GeneratesPercentagePreview)
{
    const auto wavProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-wav"_s);
    if(!wavProfile) {
        GTEST_SKIP() << "Runtime FFmpeg WAV encoder is unavailable";
    }

    const QTemporaryDir outputDir;
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

    const QTemporaryDir tempDir;
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
    Track second{track};
    second.setTitle(u"Second"_s);

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
    request.audioLoader            = &loader;
    request.encoderRegistry        = &encoderRegistry;
    request.job.tracks             = {track, second};
    request.job.preset             = preset;
    request.maxParallelConversions = 2;

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), 2);
    ASSERT_EQ(results.front().status, ConversionResultStatus::Succeeded) << results.front().error.toStdString();
    EXPECT_TRUE(QFileInfo::exists(root.filePath(u"output/cover.jpg"_s)));
    EXPECT_FALSE(QFileInfo::exists(root.filePath(u"output/notes.txt"_s)));
    EXPECT_TRUE(results.front().warnings.isEmpty());
    EXPECT_EQ(results.back().status, ConversionResultStatus::Succeeded) << results.back().error.toStdString();
    EXPECT_TRUE(results.back().warnings.isEmpty());
}

TEST(ConversionRunnerTest, GeneratesGroupedMultiTrackOutputs)
{
    const auto flacProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-flac"_s);
    if(!flacProfile) {
        GTEST_SKIP() << "Runtime FFmpeg FLAC encoder is unavailable";
    }

    const QTemporaryDir outputDir;
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
    request.audioLoader            = &loader;
    request.encoderRegistry        = &encoderRegistry;
    request.job.tracks             = {first, second, third};
    request.job.preset             = preset;
    request.maxParallelConversions = 2;

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

TEST(ConversionRunnerTest, ConvertsIndependentGroupsInParallelAndKeepsMergedOutputSequential)
{
    const auto profile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-flac"_s);
    if(!profile) {
        GTEST_SKIP() << "Runtime FFmpeg FLAC encoder is unavailable";
    }

    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QDir root{tempDir.path()};
    ASSERT_TRUE(root.mkpath(u"source"_s));
    const QString fixture = QDir{QString::fromLatin1(FOOYIN_TEST_SOURCE_DIR)}.filePath(u"data/audio/audiotest.wav"_s);
    const QString sourcePath = root.filePath(u"source/input.wav"_s);
    ASSERT_TRUE(QFile::copy(fixture, sourcePath));
    QFile cover{root.filePath(u"source/cover.jpg"_s)};
    ASSERT_TRUE(cover.open(QIODevice::WriteOnly));
    ASSERT_EQ(cover.write("cover"), 5);
    cover.close();

    const auto state = std::make_shared<ParallelDecoderState>();
    AudioLoader loader;
    loader.addDecoder(u"Parallel"_s, [state] { return std::make_unique<ParallelDecoder>(state); });
    loader.addReader(u"TagLib"_s, [] { return std::make_unique<TagLibReader>(); });
    AudioEncoderRegistry registry;
    registry.addEncoderBackend(u"ffmpeg"_s, u"FFmpeg"_s, [] { return std::make_unique<FFmpegEncoder>(); });
    std::atomic_int dspInstances{0};
    DspRegistry dspRegistry;
    dspRegistry.registerDsp({.id = u"test.dsp.counting"_s, .name = u"Counting DSP"_s, .factory = [&dspInstances] {
                                 ++dspInstances;
                                 return std::make_unique<CountingDsp>();
                             }});

    ConversionRunner::Request request;
    request.audioLoader                             = &loader;
    request.encoderRegistry                         = &registry;
    request.dspRegistry                             = &dspRegistry;
    request.maxParallelConversions                  = 2;
    auto& preset                                    = request.job.preset;
    preset.encoder.profile                          = profile->profile;
    preset.encoder.outputSampleFormat               = SampleFormat::S16;
    preset.destination.mode                         = DestinationMode::FixedFolder;
    preset.destination.fixedFolder                  = root.filePath(u"output"_s);
    preset.destination.filenamePattern              = u"%album%"_s;
    preset.destination.existingFileMode             = ExistingFileMode::Overwrite;
    preset.destination.outputStyle                  = OutputStyle::MultiTrackFiles;
    preset.processing.preserveDspStateBetweenTracks = true;
    preset.processing.dspChain                      = {{.id          = u"test.dsp.counting"_s,
                                                        .name        = u"Counting DSP"_s,
                                                        .hasSettings = false,
                                                        .enabled     = true,
                                                        .instanceId  = 1,
                                                        .settings    = {}}};
    preset.other.verifyOutput                       = true;
    preset.other.copyFilesPattern                   = u"*.jpg"_s;
    for(const auto& [album, title] : std::vector<std::pair<QString, QString>>{{u"Album A"_s, u"A1"_s},
                                                                              {u"Album B"_s, u"B1"_s},
                                                                              {u"Album A"_s, u"A2"_s},
                                                                              {u"Album C"_s, u"C1"_s},
                                                                              {u"Album B"_s, u"B2"_s}}) {
        Track track{sourcePath};
        track.setAlbum(album);
        track.setTitle(title);
        request.job.tracks.push_back(std::move(track));
    }

    const std::vector expectedTitles{u"A1"_s, u"A2"_s, u"B1"_s, u"B2"_s, u"C1"_s};
    const std::vector expectedAlbums{u"Album A"_s, u"Album A"_s, u"Album B"_s, u"Album B"_s, u"Album C"_s};
    std::vector<bool> finished(expectedTitles.size());
    request.progressCallback = [&](const ConversionProgress& progress) {
        ASSERT_GE(progress.trackIndex, 0);
        const auto i = static_cast<size_t>(progress.trackIndex);
        ASSERT_LT(i, finished.size());
        EXPECT_FALSE(finished[i]);
        EXPECT_EQ(QFileInfo{progress.outputPath}.completeBaseName(), expectedAlbums[i]);
        if(progress.finished) {
            finished[i] = true;
            EXPECT_TRUE(QFileInfo::exists(progress.outputPath));
        }
    };

    const auto results = ConversionRunner::run(request);
    ASSERT_EQ(results.size(), expectedTitles.size());
    EXPECT_EQ(state->peakActive, 2);
    EXPECT_EQ(state->active, 0);
    EXPECT_EQ(dspInstances.load(), 3);
    for(size_t i{0}; i < results.size(); ++i) {
        EXPECT_TRUE(finished[i]);
        ASSERT_EQ(results[i].status, ConversionResultStatus::Succeeded) << results[i].error.toStdString();
        EXPECT_EQ(results[i].sourceTrack.title(), expectedTitles[i]);
        EXPECT_TRUE(results[i].warnings.isEmpty());
        EXPECT_EQ(outputTitle(results[i].outputPath), i < 2 ? u"A1"_s : i < 4 ? u"B1"_s : u"C1"_s);
        EXPECT_NEAR(static_cast<double>(audioDurationMs(results[i].outputPath)), i < 4 ? 3669.0 : 1834.0, 5.0);
    }
    for(const QString& album : {u"Album A"_s, u"Album B"_s, u"Album C"_s}) {
        std::vector<QString> titles;
        for(const auto& [startedAlbum, title] : state->startedTracks) {
            if(startedAlbum == album) {
                titles.push_back(title);
            }
        }
        if(album == u"Album A"_s) {
            EXPECT_EQ(titles, (std::vector{u"A1"_s, u"A2"_s}));
        }
        else if(album == u"Album B"_s) {
            EXPECT_EQ(titles, (std::vector{u"B1"_s, u"B2"_s}));
        }
        else {
            EXPECT_EQ(titles, (std::vector{u"C1"_s}));
        }
    }
    EXPECT_TRUE(QFileInfo::exists(root.filePath(u"output/cover.jpg"_s)));

    state->waitForPeers        = false;
    state->peakActive          = 0;
    request.progressCallback   = {};
    request.sourceObserver     = std::make_shared<ThreadCheckingObserver>(std::this_thread::get_id());
    const auto observedResults = ConversionRunner::run(request);
    for(const auto& result : observedResults) {
        EXPECT_EQ(result.status, ConversionResultStatus::Succeeded) << result.error.toStdString();
    }
    EXPECT_EQ(state->peakActive, 1);
    const auto observer = std::static_pointer_cast<ThreadCheckingObserver>(request.sourceObserver);
    EXPECT_FALSE(observer->wrongThread.load());
    EXPECT_EQ(observer->tracksStarted.load(), 5);

    state->peakActive = 0;
    state->startedTracks.clear();
    dspInstances.store(0);
    request.sourceObserver.reset();
    preset.destination.outputStyle = OutputStyle::MergeTracks;
    const auto mergedResults       = ConversionRunner::run(request);
    ASSERT_EQ(mergedResults.size(), 5);
    EXPECT_EQ(state->peakActive, 1);
    EXPECT_EQ(dspInstances.load(), 1);
    for(const auto& result : mergedResults) {
        EXPECT_EQ(result.status, ConversionResultStatus::Succeeded) << result.error.toStdString();
        EXPECT_EQ(result.outputPath, mergedResults.front().outputPath);
    }
    EXPECT_NEAR(static_cast<double>(audioDurationMs(mergedResults.front().outputPath)), 9173.0, 5.0);
    std::vector<QString> mergedTitles;
    for(const auto& [album, title] : state->startedTracks) {
        if(!album.isEmpty()) {
            mergedTitles.push_back(title);
        }
    }
    std::vector<QString> inputTitles;
    for(const auto& track : request.job.tracks) {
        inputTitles.push_back(track.title());
    }
    EXPECT_EQ(mergedTitles, inputTitles);
}

TEST(ConversionRunnerTest, PreservesDspStateAcrossCombinedOutputWhenRequested)
{
    const auto wavProfile = profileById(FFmpegEncoder{}.availableEncoders(), u"ffmpeg-wav"_s);
    if(!wavProfile) {
        GTEST_SKIP() << "Runtime FFmpeg WAV encoder is unavailable";
    }

    const QTemporaryDir outputDir;
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
