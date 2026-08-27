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

#include "drive/cddrivebackend.h"

#include <gtest/gtest.h>

#include <limits>
#include <tuple>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
CdToc testToc()
{
    return {.firstTrackNumber = 1,
            .lastTrackNumber  = 2,
            .leadoutSector    = 150,
            .tracks           = {{.number = 1, .firstSector = 0, .endSectorExclusive = 75, .isAudio = true},
                                 {.number = 2, .firstSector = 75, .endSectorExclusive = 150, .isAudio = true}}};
}

class FakeDriveSession : public CdDriveSession
{
public:
    explicit FakeDriveSession(CdToc toc)
        : m_toc{std::move(toc)}
    { }

    std::expected<CdToc, CdError> readToc() override
    {
        if(m_cancelled) {
            return std::unexpected(
                CdError{.code = CdDriveError::Cancelled, .message = u"Cancelled"_s, .platformCode = 0});
        }
        return m_toc;
    }

    std::expected<CdSectorRead, CdError> readAudioSectors(int firstSector, int count) override
    {
        if(m_cancelled) {
            return std::unexpected(
                CdError{.code = CdDriveError::Cancelled, .message = u"Cancelled"_s, .platformCode = 0});
        }
        if(firstSector < 0 || count < 0 || firstSector > m_toc.leadoutSector) {
            return std::unexpected(
                CdError{.code = CdDriveError::ReadFailed, .message = u"Invalid range"_s, .platformCode = 0});
        }

        const int available = std::max(m_toc.leadoutSector - firstSector, 0);
        const int sectors   = std::min(count, available);
        return CdSectorRead{.pcm = QByteArray(sectors * BytesPerSector, '\x5a'), .sectorsRead = sectors};
    }

    void cancel() override
    {
        m_cancelled = true;
    }

private:
    CdToc m_toc;
    bool m_cancelled{false};
};

class FakeDriveBackend : public CdDriveBackend
{
public:
    std::vector<CdDriveInfo> drives() override
    {
        return {{.id                 = u"drive-0"_s,
                 .displayName        = u"Test optical drive"_s,
                 .settingsKey        = u"test:model:serial"_s,
                 .supportsSpeedLimit = true,
                 .vendor             = {},
                 .model              = {},
                 .revision           = {}}};
    }

    std::expected<OpenedDrive, CdError> open(const QString& driveId) override
    {
        if(driveId != u"drive-0"_s) {
            return std::unexpected(
                CdError{.code = CdDriveError::Unsupported, .message = u"Unknown drive"_s, .platformCode = 404});
        }
        return OpenedDrive{.drive = drives().front(), .session = std::make_unique<FakeDriveSession>(testToc())};
    }
};
} // namespace

TEST(CdDriveBackendTest, OpenedSessionReadsTocAndAudioUntilCancelled)
{
    FakeDriveBackend backend;

    const std::vector<CdDriveInfo> drives = backend.drives();
    ASSERT_EQ(1, drives.size());
    EXPECT_EQ(u"drive-0"_s, drives.front().id);
    EXPECT_TRUE(drives.front().supportsSpeedLimit);

    auto opened = backend.open(drives.front().id);
    ASSERT_TRUE(opened.has_value()) << opened.error().message.toStdString();
    EXPECT_EQ(drives.front(), opened->drive);
    std::unique_ptr<CdDriveSession> session = std::move(opened->session);

    const auto toc = session->readToc();
    ASSERT_TRUE(toc.has_value()) << toc.error().message.toStdString();
    EXPECT_EQ(testToc(), *toc);

    const auto read = session->readAudioSectors(149, 4);
    ASSERT_TRUE(read.has_value()) << read.error().message.toStdString();
    EXPECT_EQ(1, read->sectorsRead);
    EXPECT_EQ(BytesPerSector, read->pcm.size());
    EXPECT_TRUE(validateSectorRead(*read, 4).has_value());

    const auto secure = session->readAudioSectorsSecure(0, 1, CdRippingSecurity::Standard);
    ASSERT_FALSE(secure.has_value());
    EXPECT_EQ(CdDriveError::Unsupported, secure.error().code);

    const auto speed = session->setReadSpeed(4);
    ASSERT_FALSE(speed.has_value());
    EXPECT_EQ(CdDriveError::Unsupported, speed.error().code);

    session->cancel();
    const auto cancelledRead = session->readAudioSectors(0, 1);
    ASSERT_FALSE(cancelledRead.has_value());
    EXPECT_EQ(CdDriveError::Cancelled, cancelledRead.error().code);
}

TEST(CdDriveBackendTest, OpenPreservesStableErrors)
{
    FakeDriveBackend backend;
    const auto opened = backend.open(u"missing"_s);

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(CdDriveError::Unsupported, opened.error().code);
    EXPECT_EQ(u"Unknown drive"_s, opened.error().message);
    EXPECT_EQ(404, opened.error().platformCode);
}

TEST(CdDriveBackendTest, ValidatesSectorCountAndCompleteBuffers)
{
    EXPECT_TRUE(validateSectorRead({QByteArray(2 * BytesPerSector, '\0'), 2}, 2).has_value());
    EXPECT_TRUE(validateSectorRead({}, 0).has_value());

    const auto invalidBuffer = validateSectorRead({QByteArray(BytesPerSector, '\0'), 2}, 2);
    ASSERT_FALSE(invalidBuffer.has_value());
    EXPECT_TRUE(invalidBuffer.error().message.isEmpty());
    EXPECT_FALSE(validateSectorRead({QByteArray(2 * BytesPerSector, '\0'), 2}, 1).has_value());
    EXPECT_FALSE(validateSectorRead({}, -1).has_value());
}

TEST(CdDriveBackendTest, ValidatesBoundedSectorReadRequests)
{
    EXPECT_TRUE(validateSectorReadRequest(0, 0, 16).has_value());
    EXPECT_TRUE(validateSectorReadRequest(1234, 16, 16).has_value());

    for(const auto& [firstSector, count, maximum] : {
            std::tuple{-1, 1, 16},
            std::tuple{0, -1, 16},
            std::tuple{0, 17, 16},
            std::tuple{0, 1, 0},
            std::tuple{std::numeric_limits<int>::max(), 1, 16},
        }) {
        const auto result = validateSectorReadRequest(firstSector, count, maximum);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(CdDriveError::ReadFailed, result.error().code);
        EXPECT_TRUE(result.error().message.isEmpty());
    }
}
} // namespace Fooyin::Cdda
