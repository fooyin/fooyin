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

#include "cddasectorreader.h"

#include <gtest/gtest.h>

#include <cstring>
#include <expected>
#include <functional>
#include <stop_token>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
QByteArray sector(char value)
{
    return {BytesPerSector, value};
}

QByteArray sectors(std::initializer_list<char> values)
{
    QByteArray pcm;
    pcm.reserve(static_cast<qsizetype>(values.size()) * BytesPerSector);
    for(const char value : values) {
        pcm.append(sector(value));
    }
    return pcm;
}

class FakeSession : public CdDriveSession
{
public:
    using ReadHandler = std::function<std::expected<CdSectorRead, CdError>(int, int, int)>;

    explicit FakeSession(ReadHandler handler)
        : m_handler{std::move(handler)}
    { }

    std::expected<CdToc, CdError> readToc() override
    {
        return std::unexpected(CdError{
            .code = CdDriveError::Unsupported, .message = u"Not used by sector reader tests"_s, .platformCode = 0});
    }

    std::expected<CdSectorRead, CdError> readAudioSectors(int firstSector, int count) override
    {
        requests.emplace_back(firstSector, count);
        policies.push_back(CdRippingSecurity::Disabled);
        return m_handler(firstSector, count, requests.size() - 1);
    }

    std::expected<CdSectorRead, CdError> readAudioSectorsSecure(int firstSector, int count,
                                                                CdRippingSecurity security) override
    {
        requests.emplace_back(firstSector, count);
        policies.push_back(security);
        return m_handler(firstSector, count, requests.size() - 1);
    }

    void cancel() override
    {
        ++cancelCalls;
    }

    QStringList takeWarnings() override
    {
        return std::exchange(warnings, {});
    }

    std::vector<std::pair<int, int>> requests;
    std::vector<CdRippingSecurity> policies;
    QStringList warnings;
    int cancelCalls{0};

private:
    ReadHandler m_handler;
};

CdSectorRead patternedRead(int firstSector, int count)
{
    QByteArray pcm;
    pcm.reserve(static_cast<qsizetype>(count) * BytesPerSector);
    for(int current = firstSector; current < firstSector + count; ++current) {
        pcm.append(sector(static_cast<char>(current)));
    }
    return {.pcm = std::move(pcm), .sectorsRead = count};
}

CdToc mixedModeToc()
{
    return {.firstTrackNumber = 1,
            .lastTrackNumber  = 5,
            .leadoutSector    = 10,
            .tracks           = {{.number = 1, .firstSector = 0, .endSectorExclusive = 2, .isAudio = false},
                                 {.number = 2, .firstSector = 2, .endSectorExclusive = 4, .isAudio = true},
                                 {.number = 3, .firstSector = 4, .endSectorExclusive = 6, .isAudio = true},
                                 {.number = 4, .firstSector = 6, .endSectorExclusive = 8, .isAudio = false},
                                 {.number = 5, .firstSector = 8, .endSectorExclusive = 10, .isAudio = true}}};
}

CdToc audioOnlyToc()
{
    return {.firstTrackNumber = 1,
            .lastTrackNumber  = 1,
            .leadoutSector    = 2,
            .tracks           = {{.number = 1, .firstSector = 0, .endSectorExclusive = 2, .isAudio = true}}};
}

CdSectorRead indexedRead(int firstSector, int count)
{
    QByteArray pcm(static_cast<qsizetype>(count) * BytesPerSector, Qt::Uninitialized);
    for(int sectorIndex{0}; sectorIndex < count; ++sectorIndex) {
        for(int frame{0}; frame < FramesPerSector; ++frame) {
            const int32_t value = ((firstSector + sectorIndex) * FramesPerSector) + frame;
            const qsizetype byteOffset
                = ((static_cast<qsizetype>(sectorIndex) * FramesPerSector) + frame) * sizeof(value);
            std::memcpy(pcm.data() + byteOffset, &value, sizeof(value));
        }
    }
    return {.pcm = std::move(pcm), .sectorsRead = count};
}

std::vector<int32_t> frameValues(const QByteArray& pcm)
{
    std::vector<int32_t> result(static_cast<size_t>(pcm.size() / static_cast<qsizetype>(sizeof(int32_t))));
    std::memcpy(result.data(), pcm.constData(), static_cast<size_t>(pcm.size()));
    return result;
}
} // namespace

TEST(CddaSectorReaderTest, DisabledModeCompletesPartialBackendReadsWithoutUnboundedRequests)
{
    auto session = std::make_unique<FakeSession>([](int firstSector, int /*count*/, int /*request*/) {
        return std::expected<CdSectorRead, CdError>{patternedRead(firstSector, 1)};
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Disabled};

    const auto result = reader.readAudioSectors(10, 3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(3, result->sectorsRead);
    EXPECT_EQ(sectors({10, 11, 12}), result->pcm);
    EXPECT_EQ((std::vector<std::pair<int, int>>{{10, 3}, {11, 2}, {12, 1}}), session->requests);
    EXPECT_TRUE(reader.takeWarnings().empty());
}

TEST(CddaSectorReaderTest, RejectsInvalidAndUnboundedRequestsBeforeCallingBackend)
{
    auto session = std::make_unique<FakeSession>(
        [](int, int, int) { return std::expected<CdSectorRead, CdError>{CdSectorRead{}}; });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Disabled};

    const auto negativeSector = reader.readAudioSectors(-1, 1);
    ASSERT_FALSE(negativeSector.has_value());
    EXPECT_EQ(CdDriveError::ReadFailed, negativeSector.error().code);
    EXPECT_TRUE(negativeSector.error().message.isEmpty());

    const auto unbounded = reader.readAudioSectors(0, MaximumSectorsPerPolicyRead + 1);
    ASSERT_FALSE(unbounded.has_value());
    EXPECT_EQ(CdDriveError::ReadFailed, unbounded.error().code);
    EXPECT_TRUE(unbounded.error().message.isEmpty());
    EXPECT_TRUE(reader.readAudioSectors(0, 0).has_value());
    EXPECT_TRUE(session->requests.empty());
}

TEST(CddaSectorReaderTest, DelegatesSecurityPolicyAndWarningsToBackend)
{
    auto session = std::make_unique<FakeSession>([](int firstSector, int count, int) {
        return std::expected<CdSectorRead, CdError>{patternedRead(firstSector, count)};
    });

    for(const CdRippingSecurity security :
        {CdRippingSecurity::Disabled, CdRippingSecurity::Standard, CdRippingSecurity::Paranoid}) {
        CddaSectorReader reader{session.get(), security};
        session->warnings.push_back(u"Recovery diagnostic"_s);

        const auto result = reader.readAudioSectors(10, 3);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(patternedRead(10, 3).pcm, result->pcm);
        EXPECT_EQ(security, session->policies.back());
        EXPECT_EQ((QStringList{u"Recovery diagnostic"_s}), reader.takeWarnings());
        EXPECT_TRUE(reader.takeWarnings().empty());
    }
}

TEST(CddaSectorReaderTest, PropagatesBackendPolicyFailureWithoutRetrying)
{
    auto session = std::make_unique<FakeSession>([](int, int, int) -> std::expected<CdSectorRead, CdError> {
        return std::unexpected(
            CdError{.code = CdDriveError::ReadFailed, .message = u"Secure reader failed"_s, .platformCode = 5});
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Paranoid};

    const auto result = reader.readAudioSectors(20, 4);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(CdDriveError::ReadFailed, result.error().code);
    EXPECT_EQ(1, session->requests.size());
    EXPECT_EQ(CdRippingSecurity::Paranoid, session->policies.front());
}

TEST(CddaSectorReaderTest, CancellationIsObservedBetweenBackendRequests)
{
    std::stop_source cancel;
    auto session = std::make_unique<FakeSession>([&cancel](int firstSector, int count, int) {
        cancel.request_stop();
        return std::expected<CdSectorRead, CdError>{patternedRead(firstSector, count)};
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Paranoid, cancel.get_token()};

    const auto result = reader.readAudioSectors(0, 4);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(CdDriveError::Cancelled, result.error().code);
    EXPECT_EQ(1, session->requests.size());
}

TEST(CddaSectorReaderTest, CorrectedReadPreservesRequestedFrameCount)
{
    const CdToc toc = mixedModeToc();
    auto session    = std::make_unique<FakeSession>([](int firstSector, int count, int) {
        return std::expected<CdSectorRead, CdError>{indexedRead(firstSector, count)};
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Disabled};

    const auto result = reader.readCorrectedFrames(toc, toc.tracks.at(1), 100, 700, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(700, result->framesRead);
    EXPECT_EQ(700 * static_cast<int>(sizeof(int32_t)), result->pcm.size());

    const auto values = frameValues(result->pcm);
    ASSERT_EQ(700, values.size());
    EXPECT_EQ((2 * FramesPerSector) + 100, values.front());
    EXPECT_EQ((2 * FramesPerSector) + 799, values.back());
    EXPECT_TRUE(reader.takeWarnings().empty());
}

TEST(CddaSectorReaderTest, PositiveOffsetReadsLaterFramesAcrossAdjacentAudioTrack)
{
    const CdToc toc = mixedModeToc();
    auto session    = std::make_unique<FakeSession>([](int firstSector, int count, int) {
        return std::expected<CdSectorRead, CdError>{indexedRead(firstSector, count)};
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Disabled};
    const int trackFrames = 2 * FramesPerSector;

    const auto result = reader.readCorrectedFrames(toc, toc.tracks.at(1), trackFrames - 5, 5, 10);
    ASSERT_TRUE(result.has_value());

    const auto values = frameValues(result->pcm);
    ASSERT_EQ(5, values.size());
    EXPECT_EQ((4 * FramesPerSector) + 5, values.front());
    EXPECT_EQ((4 * FramesPerSector) + 9, values.back());
    EXPECT_TRUE(reader.takeWarnings().empty());
}

TEST(CddaSectorReaderTest, NegativeOffsetReadsEarlierFramesAcrossAdjacentAudioTrack)
{
    const CdToc toc = mixedModeToc();
    auto session    = std::make_unique<FakeSession>([](int firstSector, int count, int) {
        return std::expected<CdSectorRead, CdError>{indexedRead(firstSector, count)};
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Disabled};

    const auto result = reader.readCorrectedFrames(toc, toc.tracks.at(2), 0, 10, -5);
    ASSERT_TRUE(result.has_value());

    const auto values = frameValues(result->pcm);
    ASSERT_EQ(10, values.size());
    EXPECT_EQ((4 * FramesPerSector) - 5, values.front());
    EXPECT_EQ((4 * FramesPerSector) + 4, values.back());
    EXPECT_TRUE(reader.takeWarnings().empty());
}

TEST(CddaSectorReaderTest, OffsetNeverReadsAcrossPrecedingDataTrack)
{
    const CdToc toc = mixedModeToc();
    auto session    = std::make_unique<FakeSession>([](int firstSector, int count, int) {
        return std::expected<CdSectorRead, CdError>{indexedRead(firstSector, count)};
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Disabled};

    const auto result = reader.readCorrectedFrames(toc, toc.tracks.at(1), 0, 20, -10);
    ASSERT_TRUE(result.has_value());

    const auto values = frameValues(result->pcm);
    ASSERT_EQ(20, values.size());
    EXPECT_TRUE(std::ranges::all_of(values.begin(), values.begin() + 10, [](int32_t value) { return value == 0; }));
    EXPECT_EQ(2 * FramesPerSector, values.at(10));
    EXPECT_EQ((2 * FramesPerSector) + 9, values.back());
    ASSERT_EQ(1, reader.takeWarnings().size());
    ASSERT_FALSE(session->requests.empty());
    EXPECT_GE(session->requests.front().first, 2);
}

TEST(CddaSectorReaderTest, OffsetNeverReadsAcrossFollowingDataTrack)
{
    const CdToc toc = mixedModeToc();
    auto session    = std::make_unique<FakeSession>([](int firstSector, int count, int) {
        return std::expected<CdSectorRead, CdError>{indexedRead(firstSector, count)};
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Disabled};
    const int trackFrames = 2 * FramesPerSector;

    const auto result = reader.readCorrectedFrames(toc, toc.tracks.at(2), trackFrames - 10, 10, 5);
    ASSERT_TRUE(result.has_value());

    const auto values = frameValues(result->pcm);
    ASSERT_EQ(10, values.size());
    EXPECT_EQ((6 * FramesPerSector) - 5, values.front());
    EXPECT_EQ((6 * FramesPerSector) - 1, values.at(4));
    EXPECT_TRUE(std::ranges::all_of(values.begin() + 5, values.end(), [](int32_t value) { return value == 0; }));
    ASSERT_EQ(1, reader.takeWarnings().size());
    ASSERT_FALSE(session->requests.empty());
    EXPECT_LE(session->requests.back().first + session->requests.back().second, 6);
}

TEST(CddaSectorReaderTest, UnavailableCorrectionReturnsExactSilenceWithoutDriveRead)
{
    const CdToc toc = mixedModeToc();
    auto session    = std::make_unique<FakeSession>([](int firstSector, int count, int) {
        return std::expected<CdSectorRead, CdError>{indexedRead(firstSector, count)};
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Disabled};

    const auto result = reader.readCorrectedFrames(toc, toc.tracks.at(4), 0, 32, -100000);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(32, result->framesRead);
    EXPECT_EQ(QByteArray(32 * static_cast<int>(sizeof(int32_t)), '\0'), result->pcm);
    EXPECT_TRUE(session->requests.empty());
    ASSERT_EQ(1, reader.takeWarnings().size());
}

TEST(CddaSectorReaderTest, CorrectionPadsAtDiscBeginningAndEnd)
{
    const CdToc toc = audioOnlyToc();
    auto session    = std::make_unique<FakeSession>([](int firstSector, int count, int) {
        return std::expected<CdSectorRead, CdError>{indexedRead(firstSector, count)};
    });
    CddaSectorReader reader{session.get(), CdRippingSecurity::Disabled};
    const int trackFrames = 2 * FramesPerSector;

    const auto beginning = reader.readCorrectedFrames(toc, toc.tracks.front(), 0, 10, -5);
    ASSERT_TRUE(beginning.has_value());
    const auto beginningValues = frameValues(beginning->pcm);
    ASSERT_EQ(10, beginningValues.size());
    EXPECT_TRUE(std::ranges::all_of(beginningValues.begin(), beginningValues.begin() + 5,
                                    [](int32_t value) { return value == 0; }));
    EXPECT_EQ(0, beginningValues.at(5));
    EXPECT_EQ(4, beginningValues.back());

    const auto ending = reader.readCorrectedFrames(toc, toc.tracks.front(), trackFrames - 10, 10, 5);
    ASSERT_TRUE(ending.has_value());
    const auto endingValues = frameValues(ending->pcm);
    ASSERT_EQ(10, endingValues.size());
    EXPECT_EQ(trackFrames - 5, endingValues.front());
    EXPECT_EQ(trackFrames - 1, endingValues.at(4));
    EXPECT_TRUE(
        std::ranges::all_of(endingValues.begin() + 5, endingValues.end(), [](int32_t value) { return value == 0; }));

    const QStringList warnings = reader.takeWarnings();
    ASSERT_EQ(2, warnings.size());
    EXPECT_TRUE(warnings.at(0).contains(u"before"_s));
    EXPECT_TRUE(warnings.at(1).contains(u"after"_s));
}
} // namespace Fooyin::Cdda
