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

#include "cddadecoder.h"

#include "cddatoc.h"
#include "cddaurl.h"

#include <gtest/gtest.h>

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <optional>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
CdToc testToc()
{
    return {.firstTrackNumber = 1,
            .lastTrackNumber  = 3,
            .leadoutSector    = 155,
            .tracks           = {{.number = 1, .firstSector = 0, .endSectorExclusive = 2, .isAudio = false},
                                 {.number = 2, .firstSector = 2, .endSectorExclusive = 5, .isAudio = true},
                                 {.number = 3, .firstSector = 5, .endSectorExclusive = 155, .isAudio = true}}};
}

struct BackendState
{
    CdToc toc{testToc()};
    std::optional<CdError> audioError;
    bool returnInvalidBuffer{false};
    int openCalls{0};
    int tocCalls{0};
    int cancelCalls{0};
    std::vector<std::pair<int, int>> reads;
    std::vector<int> readSpeeds;

    std::mutex mutex;
    std::condition_variable condition;
    bool blockAudio{false};
    bool audioEntered{false};
    bool cancelled{false};
};

class FakeSession : public CdDriveSession
{
public:
    explicit FakeSession(std::shared_ptr<BackendState> state)
        : m_state{std::move(state)}
    { }

    std::expected<CdToc, CdError> readToc() override
    {
        ++m_state->tocCalls;
        return m_state->toc;
    }

    std::expected<CdSectorRead, CdError> readAudioSectors(int firstSector, int count) override
    {
        {
            std::unique_lock lock{m_state->mutex};

            m_state->reads.emplace_back(firstSector, count);
            m_state->audioEntered = true;
            m_state->condition.notify_all();
            m_state->condition.wait(lock, [&] { return !m_state->blockAudio || m_state->cancelled; });

            if(m_state->cancelled) {
                return std::unexpected(
                    CdError{.code = CdDriveError::Cancelled, .message = u"Cancelled by test"_s, .platformCode = 0});
            }
        }

        if(m_state->audioError) {
            return std::unexpected(*m_state->audioError);
        }
        if(m_state->returnInvalidBuffer) {
            return CdSectorRead{.pcm = QByteArray(BytesPerSector, '\0'), .sectorsRead = count};
        }

        QByteArray pcm;
        pcm.reserve(static_cast<qsizetype>(count) * BytesPerSector);

        for(int sector{firstSector}; sector < firstSector + count; ++sector) {
            pcm.append(QByteArray(BytesPerSector, static_cast<char>(sector & 0xFF)));
        }

        return CdSectorRead{.pcm = std::move(pcm), .sectorsRead = count};
    }

    std::expected<void, CdError> setReadSpeed(int speed) override
    {
        m_state->readSpeeds.push_back(speed);
        return {};
    }

    void cancel() override
    {
        const std::scoped_lock lock{m_state->mutex};

        ++m_state->cancelCalls;
        m_state->cancelled = true;
        m_state->condition.notify_all();
    }

private:
    std::shared_ptr<BackendState> m_state;
};

class FakeBackend : public CdDriveBackend
{
public:
    explicit FakeBackend(std::shared_ptr<BackendState> state)
        : m_state{std::move(state)}
    { }

    std::vector<CdDriveInfo> drives() override
    {
        return {{.id                 = u"drive-a"_s,
                 .displayName        = u"Test drive"_s,
                 .settingsKey        = u"test-drive"_s,
                 .supportsSpeedLimit = true,
                 .vendor             = {},
                 .model              = {},
                 .revision           = {}}};
    }

    std::expected<OpenedDrive, CdError> open(const QString& driveId) override
    {
        if(driveId != u"drive-a"_s) {
            return std::unexpected(
                CdError{.code = CdDriveError::Unsupported, .message = u"Unknown drive"_s, .platformCode = 0});
        }

        ++m_state->openCalls;
        return OpenedDrive{.drive = drives().front(), .session = std::make_unique<FakeSession>(m_state)};
    }

private:
    std::shared_ptr<BackendState> m_state;
};

class FakeSettingsProvider : public CdDriveSettingsProvider
{
public:
    CdDriveSettings settings;
    mutable std::vector<CdDriveInfo> requests;

    CdDriveSettings settingsForDrive(const CdDriveInfo& drive) const override
    {
        requests.push_back(drive);
        return settings;
    }
};

struct DecoderFixture
{
    DecoderFixture()
        : state{std::make_shared<BackendState>()}
        , discId{musicBrainzDiscId(state->toc).value()}
        , filepath{cddaUrl(discId)}
        , manager{std::make_shared<CdDriveManager>(std::make_unique<FakeBackend>(state))}
        , decoder{manager}
    { }

    bool init(int subsong)
    {
        return decoder.init({.filepath = filepath}, Track{filepath, subsong}, AudioDecoder::None).has_value();
    }

    std::shared_ptr<BackendState> state;
    QString discId;
    QString filepath;
    std::shared_ptr<CdDriveManager> manager;
    CddaDecoder decoder;
};
} // namespace

TEST(CddaDecoderTest, InitialisesWithoutTakingReadLease)
{
    DecoderFixture fixture;
    EXPECT_TRUE(fixture.decoder.extensions().isEmpty());
    EXPECT_EQ((QStringList{u"cdda"_s}), fixture.decoder.supportedSchemes());
    EXPECT_FALSE(fixture.decoder.isSeekable());
    EXPECT_FALSE(fixture.decoder.allowsConcurrentDecoding());

    ASSERT_TRUE(fixture.init(0));
    EXPECT_TRUE(fixture.decoder.isSeekable());
    EXPECT_EQ(1, fixture.state->openCalls);

    const auto format
        = fixture.decoder.init({.filepath = fixture.filepath}, Track{fixture.filepath, 0}, AudioDecoder::None);
    ASSERT_TRUE(format.has_value());
    EXPECT_EQ((AudioFormat{SampleFormat::S16, 44100, 2}), *format);
    EXPECT_EQ(1, fixture.state->openCalls);
}

TEST(CddaDecoderTest, RequestsPlaybackReserveForReads)
{
    const DecoderFixture fixture;

    EXPECT_EQ(1000, fixture.decoder.playbackPrebufferMs());
    EXPECT_FALSE(fixture.decoder.allowsConcurrentDecoding());
}

TEST(CddaDecoderTest, BuffersPartialReadsAndStopsAtTrackBoundary)
{
    DecoderFixture fixture;
    ASSERT_TRUE(fixture.init(0));

    auto first = fixture.decoder.readAudio(1000);
    ASSERT_EQ(AudioDecoder::ReadStatus::DecodedAudio, first.status);
    EXPECT_EQ(1000, first.buffer.byteCount());
    EXPECT_EQ(0, first.buffer.startTime());
    EXPECT_EQ(2, fixture.state->openCalls);
    ASSERT_EQ(1, fixture.state->reads.size());
    EXPECT_EQ(std::pair(2, 1), fixture.state->reads.front());
    EXPECT_EQ(std::byte{2}, first.buffer.constData().front());

    auto second = fixture.decoder.readAudio(2000);
    ASSERT_EQ(AudioDecoder::ReadStatus::DecodedAudio, second.status);
    EXPECT_EQ(2000, second.buffer.byteCount());
    EXPECT_EQ(5, second.buffer.startTime());
    EXPECT_EQ(std::byte{2}, second.buffer.constData().front());
    EXPECT_EQ(std::byte{3}, second.buffer.constData().back());

    auto remainder = fixture.decoder.readAudio(10000);
    ASSERT_EQ(AudioDecoder::ReadStatus::DecodedAudio, remainder.status);
    EXPECT_EQ((3 * BytesPerSector) - 3000, remainder.buffer.byteCount());
    EXPECT_EQ(std::byte{4}, remainder.buffer.constData().back());
    EXPECT_EQ(AudioDecoder::ReadStatus::EndOfStream, fixture.decoder.readAudio(4096).status);

    CddaDecoder next{fixture.manager};
    ASSERT_TRUE(next.init({.filepath = fixture.filepath}, Track{fixture.filepath, 1}, AudioDecoder::None));
    EXPECT_EQ(AudioDecoder::ReadStatus::DecodedAudio, next.readAudio(4096).status);
    EXPECT_TRUE(std::ranges::all_of(fixture.state->reads,
                                    [](const auto& read) { return read.second > 0 && read.second <= 16; }));
}

TEST(CddaDecoderTest, SeeksAtSectorGranularityAndClampsToTrackEnd)
{
    DecoderFixture fixture;
    ASSERT_TRUE(fixture.init(1));

    fixture.decoder.seek(1000);
    auto result = fixture.decoder.readAudio(BytesPerSector);
    ASSERT_EQ(AudioDecoder::ReadStatus::DecodedAudio, result.status);
    EXPECT_EQ(1000, result.buffer.startTime());
    ASSERT_FALSE(fixture.state->reads.empty());
    EXPECT_EQ(80, fixture.state->reads.front().first);

    fixture.decoder.seek(999999);
    EXPECT_EQ(AudioDecoder::ReadStatus::EndOfStream, fixture.decoder.readAudio(4096).status);
}

TEST(CddaDecoderTest, ReportsBackendAndBufferFailures)
{
    DecoderFixture fixture;
    ASSERT_TRUE(fixture.init(0));
    fixture.state->audioError
        = CdError{.code = CdDriveError::ReadFailed, .message = u"Synthetic read failure"_s, .platformCode = 5};

    auto result = fixture.decoder.readAudio(4096);
    EXPECT_EQ(AudioDecoder::ReadStatus::Error, result.status);
    EXPECT_EQ(u"Synthetic read failure"_s, result.error);

    DecoderFixture invalidBuffer;
    ASSERT_TRUE(invalidBuffer.init(0));
    invalidBuffer.state->returnInvalidBuffer = true;
    result                                   = invalidBuffer.decoder.readAudio(4096);
    EXPECT_EQ(AudioDecoder::ReadStatus::Error, result.status);
    EXPECT_TRUE(result.error.isEmpty());
}

TEST(CddaDecoderTest, AbortInterruptsBlockedSectorRead)
{
    using namespace std::chrono_literals;

    DecoderFixture fixture;
    ASSERT_TRUE(fixture.init(0));
    fixture.state->blockAudio = true;

    auto pending = std::async(std::launch::async, [&] { return fixture.decoder.readAudio(4096); });
    bool audioEntered{false};
    {
        std::unique_lock lock{fixture.state->mutex};
        audioEntered = fixture.state->condition.wait_for(lock, 1s, [&] { return fixture.state->audioEntered; });
    }
    if(!audioEntered) {
        fixture.decoder.requestAbort();
    }
    ASSERT_TRUE(audioEntered);

    fixture.decoder.requestAbort();
    ASSERT_EQ(std::future_status::ready, pending.wait_for(1s));
    const auto result = pending.get();
    EXPECT_EQ(AudioDecoder::ReadStatus::Error, result.status);
    EXPECT_EQ(u"Cancelled by test"_s, result.error);
    EXPECT_EQ(1, fixture.state->cancelCalls);
}

TEST(CddaDecoderTest, StopReleasesDriveLease)
{
    auto state             = std::make_shared<BackendState>();
    const QString discId   = musicBrainzDiscId(state->toc).value();
    const QString filepath = cddaUrl(discId);
    auto manager           = std::make_shared<CdDriveManager>(std::make_unique<FakeBackend>(state));

    CddaDecoder first{manager};
    CddaDecoder second{manager};
    ASSERT_TRUE(first.init({.filepath = filepath}, Track{filepath, 0}, AudioDecoder::None));
    ASSERT_TRUE(second.init({.filepath = filepath}, Track{filepath, 1}, AudioDecoder::None));
    ASSERT_EQ(AudioDecoder::ReadStatus::DecodedAudio, first.readAudio(4096).status);
    EXPECT_EQ(AudioDecoder::ReadStatus::Error, second.readAudio(4096).status);

    first.stop();
    CddaDecoder retry{manager};
    ASSERT_TRUE(retry.init({.filepath = filepath}, Track{filepath, 1}, AudioDecoder::None));
    EXPECT_EQ(AudioDecoder::ReadStatus::DecodedAudio, retry.readAudio(4096).status);
}

TEST(CddaDecoderTest, AppliesDriveSettingsOnlyForConversion)
{
    auto state             = std::make_shared<BackendState>();
    const QString discId   = musicBrainzDiscId(state->toc).value();
    const QString filepath = cddaUrl(discId);
    auto manager           = std::make_shared<CdDriveManager>(std::make_unique<FakeBackend>(state));
    auto settings          = std::make_shared<FakeSettingsProvider>();
    settings->settings     = {.readOffsetFrames = -10, .security = CdRippingSecurity::Disabled, .readSpeedLimit = 4};

    CddaDecoder playback{manager, settings};
    ASSERT_TRUE(playback.init({.filepath = filepath}, Track{filepath, 0}, AudioDecoder::None));
    auto playbackResult = playback.readAudio(80);
    ASSERT_EQ(AudioDecoder::ReadStatus::DecodedAudio, playbackResult.status);
    EXPECT_EQ(std::byte{2}, playbackResult.buffer.constData().front());
    EXPECT_TRUE(settings->requests.empty());
    EXPECT_TRUE(state->readSpeeds.empty());
    playback.stop();

    state->reads.clear();
    CddaDecoder conversion{manager, settings};
    ASSERT_TRUE(conversion.init({.filepath = filepath}, Track{filepath, 0}, AudioDecoder::ForConversion));
    auto conversionResult = conversion.readAudio(80);
    ASSERT_EQ(AudioDecoder::ReadStatus::DecodedAudio, conversionResult.status);
    ASSERT_EQ(80, conversionResult.buffer.byteCount());
    EXPECT_TRUE(std::ranges::all_of(conversionResult.buffer.constData().first(40),
                                    [](std::byte value) { return value == std::byte{0}; }));
    EXPECT_EQ(std::byte{2}, conversionResult.buffer.constData()[40]);
    ASSERT_EQ(1, settings->requests.size());
    EXPECT_EQ(u"test-drive"_s, settings->requests.front().settingsKey);
    EXPECT_EQ((std::vector<int>{4}), state->readSpeeds);
}

TEST(CddaDecoderTest, PropagatesExtractionWarningsOnce)
{
    auto state             = std::make_shared<BackendState>();
    const QString discId   = musicBrainzDiscId(state->toc).value();
    const QString filepath = cddaUrl(discId);
    auto manager           = std::make_shared<CdDriveManager>(std::make_unique<FakeBackend>(state));
    auto settings          = std::make_shared<FakeSettingsProvider>();
    settings->settings     = {.readOffsetFrames = -10, .security = CdRippingSecurity::Disabled};

    CddaDecoder decoder{manager, settings};
    ASSERT_TRUE(decoder.init({.filepath = filepath}, Track{filepath, 0}, AudioDecoder::ForConversion));
    ASSERT_EQ(AudioDecoder::ReadStatus::DecodedAudio, decoder.readAudio(80).status);
    const QStringList warnings = decoder.takeWarnings();
    ASSERT_FALSE(warnings.isEmpty());
    EXPECT_TRUE(std::ranges::any_of(warnings, [](const QString& warning) { return warning.contains(u"silence"_s); }));
    EXPECT_TRUE(decoder.takeWarnings().isEmpty());
}
} // namespace Fooyin::Cdda
