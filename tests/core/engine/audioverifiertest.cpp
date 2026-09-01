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
 */

#include "testutils.h"

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
        return m_format;
    }

    void seek(uint64_t) override { }
    void stop() override { }

    ReadResult readAudio(size_t) override
    {
        if(m_state->failRead) {
            return ReadResult::errorResult(u"Synthetic checksum failure"_s);
        }
        if(m_read) {
            return ReadResult::endOfStream();
        }
        m_read = true;
        AudioBuffer buffer{m_format, 0};
        buffer.resize(static_cast<size_t>(m_format.bytesForFrames(588)));
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
    EXPECT_EQ(results.front().error, u"Synthetic checksum failure"_s);
}
} // namespace
} // namespace Fooyin::Testing
