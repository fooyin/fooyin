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

#include "testutils.h"

#include <core/constants.h>
#include <core/engine/verification/audioverifier.h>

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
QCoreApplication* ensureCoreApplication()
{
    QStandardPaths::setTestModeEnabled(true);
    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    static int argc{1};
    static char appName[] = "fooyin-audioverifier-test";
    static char* argv[]   = {appName, nullptr};
    static QCoreApplication app{argc, argv};
    return &app;
}

struct DecoderState
{
    AudioDecoder::DecoderOptions options{AudioDecoder::None};
    bool failRead{false};
    int sampleRate{44100};
    int frameCount{588};
};

class VerificationDecoder : public AudioDecoder
{
public:
    explicit VerificationDecoder(std::shared_ptr<DecoderState> state)
        : m_state{std::move(state)}
    { }

    QStringList extensions() const override
    {
        return {u"verify"_s};
    }

    bool isSeekable() const override
    {
        return true;
    }

    std::optional<AudioFormat> init(const AudioSource&, const Track&, DecoderOptions options) override
    {
        m_state->options = options;
        m_read           = false;
        m_format         = {SampleFormat::S16, m_state->sampleRate, 2};
        return m_format;
    }

    void seek(uint64_t) override { }
    void stop() override { }

    ReadResult readAudio(size_t) override
    {
        if(m_state->failRead) {
            return ReadResult::errorResult(u"Synthetic checksum failure"_s);
        }
        if(m_read || m_state->frameCount == 0) {
            return ReadResult::endOfStream();
        }
        m_read = true;
        AudioBuffer buffer{m_format, 0};
        buffer.resize(m_format.bytesForFrames(m_state->frameCount));
        buffer.fillSilence();
        return ReadResult::data(std::move(buffer));
    }

    AudioBuffer readBuffer(size_t) override
    {
        return {};
    }

private:
    std::shared_ptr<DecoderState> m_state;
    AudioFormat m_format{SampleFormat::S16, 44100, 2};
    bool m_read{false};
};

class AudioVerifierTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ensureCoreApplication();
    }

    void SetUp() override
    {
        ASSERT_TRUE(m_directory.isValid());
        m_path = m_directory.filePath(u"source.verify"_s);
        QFile file{m_path};
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(file.write("data"), 4);

        m_loader.addDecoder(u"Verification decoder"_s,
                            [state = m_state] { return std::make_unique<VerificationDecoder>(state); });
    }

    QTemporaryDir m_directory;
    QString m_path;
    AudioLoader m_loader;
    std::shared_ptr<DecoderState> m_state{std::make_shared<DecoderState>()};
};

TEST_F(AudioVerifierTest, DecodesEntireTrackWithStrictIntegrityOption)
{
    const auto results = AudioVerifier::run({.audioLoader      = &m_loader,
                                             .tracks           = {Track{m_path}},
                                             .verifyIntegrity  = true,
                                             .progressCallback = {},
                                             .cancelCallback   = {},
                                             .observer         = {}});

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results.front().status, AudioVerificationStatus::Succeeded);
    EXPECT_EQ(results.front().decodedFrames, 588);
    EXPECT_EQ(results.front().md5.toHex().toUpper(), "9E297EFC7A522480EF89A4A7F39CE560");
    EXPECT_EQ(results.front().crc32, 0xBE97CE3FU);
    EXPECT_TRUE(m_state->options.testFlag(AudioDecoder::VerifyIntegrity));
    EXPECT_TRUE(m_state->options.testFlag(AudioDecoder::ForConversion));
}

TEST_F(AudioVerifierTest, UsesDecodedSampleRateForCueBoundaries)
{
    AudioVerifier::Request request;
    request.audioLoader = &m_loader;

    for(const int sampleRate : {22050, 32000, 44100, 48000, 96000}) {
        SCOPED_TRACE(sampleRate);
        m_state->sampleRate = sampleRate;
        m_state->frameCount = 2 * sampleRate / 75;

        Track cueTrack{m_path};
        cueTrack.setCuePath(u"source.cue"_s);
        cueTrack.setExtraProperty(QString::fromLatin1(Constants::CueIndex01Sector), u"1"_s);
        cueTrack.setExtraProperty(QString::fromLatin1(Constants::CueEndSector), u"2"_s);

        request.tracks        = {cueTrack};
        const auto cueResults = AudioVerifier::run(request);
        ASSERT_EQ(cueResults.size(), 1);
        EXPECT_EQ(cueResults.front().status, AudioVerificationStatus::Succeeded);
        EXPECT_EQ(cueResults.front().decodedFrames, m_state->frameCount - sampleRate / 75);
    }
}

TEST_F(AudioVerifierTest, AcceptsRoundedChapterEndAtEndOfStream)
{
    Track chapter{m_path};
    chapter.setIsChapter(true);
    chapter.setDuration(14);

    AudioVerifier::Request request;
    request.audioLoader = &m_loader;
    request.tracks      = {chapter};

    const auto chapterResults = AudioVerifier::run(request);
    ASSERT_EQ(chapterResults.size(), 1);
    EXPECT_EQ(chapterResults.front().status, AudioVerificationStatus::Succeeded);
    EXPECT_EQ(chapterResults.front().decodedFrames, 588);
    EXPECT_EQ(chapterResults.front().md5.toHex().toUpper(), "9E297EFC7A522480EF89A4A7F39CE560");
}

TEST_F(AudioVerifierTest, ReportsDecoderIntegrityFailure)
{
    m_state->failRead = true;

    const auto results = AudioVerifier::run({.audioLoader      = &m_loader,
                                             .tracks           = {Track{m_path}},
                                             .verifyIntegrity  = true,
                                             .progressCallback = {},
                                             .cancelCallback   = {},
                                             .observer         = {}});

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results.front().status, AudioVerificationStatus::Failed);
    EXPECT_TRUE(results.front().md5.isEmpty());
    EXPECT_EQ(results.front().crc32, 0U);
    EXPECT_FALSE(results.front().error.isEmpty());
}

TEST_F(AudioVerifierTest, PreservesDecoderIntegrityFailureForChapters)
{
    m_state->failRead = true;

    Track chapter{m_path};
    chapter.setIsChapter(true);
    chapter.setDuration(14);

    AudioVerifier::Request request;
    request.audioLoader = &m_loader;
    request.tracks      = {chapter};

    const auto failedChapter = AudioVerifier::run(request);
    ASSERT_EQ(failedChapter.size(), 1);
    EXPECT_EQ(failedChapter.front().status, AudioVerificationStatus::Failed);
    EXPECT_FALSE(failedChapter.front().error.isEmpty());
}

TEST_F(AudioVerifierTest, RejectsChapterShortfallBeyondRoundingTolerance)
{
    Track chapter{m_path};
    chapter.setIsChapter(true);
    chapter.setDuration(15);

    AudioVerifier::Request request;
    request.audioLoader = &m_loader;
    request.tracks      = {chapter};

    const auto shortChapter = AudioVerifier::run(request);
    ASSERT_EQ(shortChapter.size(), 1);
    EXPECT_EQ(shortChapter.front().status, AudioVerificationStatus::Failed);
    EXPECT_FALSE(shortChapter.front().error.isEmpty());
}

TEST_F(AudioVerifierTest, RejectsEmptyChapterWithinRoundingTolerance)
{
    m_state->frameCount = 0;

    Track chapter{m_path};
    chapter.setIsChapter(true);
    chapter.setDuration(1);

    AudioVerifier::Request request;
    request.audioLoader = &m_loader;
    request.tracks      = {chapter};

    const auto emptyChapter = AudioVerifier::run(request);
    ASSERT_EQ(emptyChapter.size(), 1);
    EXPECT_EQ(emptyChapter.front().status, AudioVerificationStatus::Failed);
    EXPECT_FALSE(emptyChapter.front().error.isEmpty());
}

TEST_F(AudioVerifierTest, RejectsCueSegmentMissingOneFrame)
{
    m_state->frameCount = 587;

    Track cueTrack{m_path};
    cueTrack.setCuePath(u"source.cue"_s);
    cueTrack.setExtraProperty(QString::fromLatin1(Constants::CueIndex01Sector), u"0"_s);
    cueTrack.setExtraProperty(QString::fromLatin1(Constants::CueEndSector), u"1"_s);

    AudioVerifier::Request request;
    request.audioLoader = &m_loader;
    request.tracks      = {cueTrack};

    const auto shortCue = AudioVerifier::run(request);
    ASSERT_EQ(shortCue.size(), 1);
    EXPECT_EQ(shortCue.front().status, AudioVerificationStatus::Failed);
    EXPECT_FALSE(shortCue.front().error.isEmpty());
}
} // namespace
} // namespace Fooyin::Testing
