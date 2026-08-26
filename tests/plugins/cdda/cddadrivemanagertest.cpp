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

#include "cddadrivemanager.h"

#include "cddatoc.h"
#include "cddaurl.h"

#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <thread>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
CdToc makeToc(int secondTrackStart = 100, int leadout = 200)
{
    return {.firstTrackNumber = 1,
            .lastTrackNumber  = 2,
            .leadoutSector    = leadout,
            .tracks = {{.number = 1, .firstSector = 0, .endSectorExclusive = secondTrackStart, .isAudio = true},
                       {.number = 2, .firstSector = secondTrackStart, .endSectorExclusive = leadout, .isAudio = true}}};
}

struct FakeDriveState
{
    CdDriveInfo info;
    CdToc toc;
    CdText cdText;
    std::optional<CdError> error;
    int openCalls{0};
    int tocCalls{0};
    int cdTextCalls{0};
    std::function<void()> sessionDestroyed;
    std::function<void()> tocReadStarted;
};

class FakeSession : public CdDriveSession
{
public:
    explicit FakeSession(std::shared_ptr<FakeDriveState> state)
        : m_state{std::move(state)}
    { }

    ~FakeSession() override
    {
        if(m_state->sessionDestroyed) {
            m_state->sessionDestroyed();
        }
    }

    std::expected<CdToc, CdError> readToc() override
    {
        if(m_state->tocReadStarted) {
            m_state->tocReadStarted();
        }
        ++m_state->tocCalls;
        if(m_state->error) {
            return std::unexpected(*m_state->error);
        }
        return m_state->toc;
    }

    CdText readCdText(const CdToc& /*toc*/) override
    {
        ++m_state->cdTextCalls;
        return m_state->cdText;
    }

    std::expected<CdSectorRead, CdError> readAudioSectors(int /*firstSector*/, int count) override
    {
        return CdSectorRead{.pcm = QByteArray(count * BytesPerSector, '\0'), .sectorsRead = count};
    }

    void cancel() override { }

private:
    std::shared_ptr<FakeDriveState> m_state;
};

class FakeBackend : public CdDriveBackend
{
public:
    FakeDriveState& addDrive(const QString& id, const CdToc& toc)
    {
        auto state          = std::make_shared<FakeDriveState>();
        state->info         = {.id                 = id,
                               .displayName        = id,
                               .settingsKey        = id,
                               .supportsSpeedLimit = false,
                               .vendor             = {},
                               .model              = {},
                               .revision           = {}};
        state->toc          = toc;
        FakeDriveState* ptr = state.get();
        m_drives.emplace(id, state);
        return *ptr;
    }

    std::vector<CdDriveInfo> drives() override
    {
        std::vector<CdDriveInfo> result;
        for(const auto& [id, state] : std::as_const(m_drives)) {
            Q_UNUSED(id);
            result.push_back(state->info);
        }
        std::ranges::sort(result, {}, &CdDriveInfo::id);
        return result;
    }

    std::expected<OpenedDrive, CdError> open(const QString& driveId) override
    {
        const auto it = m_drives.find(driveId);
        if(it == m_drives.end()) {
            return std::unexpected(
                CdError{.code = CdDriveError::Unsupported, .message = u"Missing drive"_s, .platformCode = 0});
        }
        ++it->second->openCalls;
        return OpenedDrive{.drive = it->second->info, .session = std::make_unique<FakeSession>(it->second)};
    }

private:
    std::map<QString, std::shared_ptr<FakeDriveState>> m_drives;
};

struct ManagerFixture
{
    ManagerFixture()
    {
        auto ownedBackend = std::make_unique<FakeBackend>();
        backend           = ownedBackend.get();
        manager           = std::make_unique<CdDriveManager>(std::move(ownedBackend), std::chrono::hours{1});
    }

    FakeBackend* backend{nullptr};
    std::unique_ptr<CdDriveManager> manager;
};
} // namespace

TEST(CdDriveManagerTest, ObservesDiscsErrorsAndStableMediaGenerations)
{
    ManagerFixture fixture;
    auto& ready             = fixture.backend->addDrive(u"drive-a"_s, makeToc());
    ready.cdText.disc.title = u"Album title"_s;
    auto& empty             = fixture.backend->addDrive(u"drive-b"_s, makeToc());
    empty.error             = CdError{.code = CdDriveError::NoMedia, .message = u"No disc"_s, .platformCode = 0};

    const auto first = fixture.manager->observations(true);
    ASSERT_EQ(2, first.size());
    const auto readyIt = std::ranges::find(first, u"drive-a"_s, [](const auto& item) { return item.drive.id; });
    ASSERT_NE(first.end(), readyIt);
    EXPECT_TRUE(readyIt->toc.has_value());
    EXPECT_TRUE(isValidDiscId(readyIt->discId));
    EXPECT_FALSE(readyIt->cdText.has_value());
    EXPECT_EQ(0, ready.cdTextCalls);

    const uint64_t generation = readyIt->generation;

    const auto cdText = fixture.manager->readCdText(*readyIt);
    ASSERT_TRUE(cdText.has_value());
    EXPECT_EQ(u"Album title"_s, cdText->disc.title);
    EXPECT_EQ(1, ready.cdTextCalls);

    const auto unchanged   = fixture.manager->observations(true);
    const auto unchangedIt = std::ranges::find(unchanged, u"drive-a"_s, [](const auto& item) { return item.drive.id; });
    ASSERT_NE(unchanged.end(), unchangedIt);
    EXPECT_EQ(generation, unchangedIt->generation);
    ASSERT_TRUE(unchangedIt->cdText.has_value());
    EXPECT_EQ(u"Album title"_s, unchangedIt->cdText->disc.title);
    EXPECT_EQ(1, ready.cdTextCalls);

    ready.toc            = makeToc(120, 240);
    const auto changed   = fixture.manager->observations(true);
    const auto changedIt = std::ranges::find(changed, u"drive-a"_s, [](const auto& item) { return item.drive.id; });
    ASSERT_NE(changed.end(), changedIt);
    EXPECT_NE(generation, changedIt->generation);
    EXPECT_FALSE(changedIt->cdText.has_value());
}

TEST(CdDriveManagerTest, EnumeratesWithoutOpeningDrivesAndInspectsOnlyTheSelection)
{
    ManagerFixture fixture;
    auto& first  = fixture.backend->addDrive(u"drive-a"_s, makeToc());
    auto& second = fixture.backend->addDrive(u"drive-b"_s, makeToc(120, 240));

    const std::vector<CdDriveInfo> drives = fixture.manager->drives();
    ASSERT_EQ(2, drives.size());
    EXPECT_EQ(0, first.openCalls);
    EXPECT_EQ(0, second.openCalls);

    const CdDriveObservation observation = fixture.manager->observeDrive(drives.front());
    EXPECT_EQ(u"drive-a"_s, observation.drive.id);
    EXPECT_TRUE(observation.toc.has_value());
    EXPECT_GT(observation.generation, 0U);
    EXPECT_EQ(1, first.openCalls);
    EXPECT_EQ(0, second.openCalls);
}

TEST(CdDriveManagerTest, UsesShortCacheAndInvalidationRefreshesTheCompleteDriveSet)
{
    ManagerFixture fixture;
    auto& first  = fixture.backend->addDrive(u"drive-a"_s, makeToc());
    auto& second = fixture.backend->addDrive(u"drive-b"_s, makeToc(120, 240));

    ASSERT_EQ(2, fixture.manager->observations().size());
    EXPECT_EQ(1, first.openCalls);
    EXPECT_EQ(1, second.openCalls);

    ASSERT_EQ(2, fixture.manager->observations().size());
    EXPECT_EQ(1, first.openCalls);
    EXPECT_EQ(1, second.openCalls);

    fixture.manager->invalidateDrive(u"drive-a"_s);
    ASSERT_EQ(2, fixture.manager->observations().size());
    EXPECT_EQ(2, first.openCalls);
    EXPECT_EQ(2, second.openCalls);
}

TEST(CdDriveManagerTest, ResolvesPreferredDriveAndFallsBackToAnotherMatchingDrive)
{
    ManagerFixture fixture;
    const CdToc toc = makeToc();
    fixture.backend->addDrive(u"drive-a"_s, toc);
    auto& second         = fixture.backend->addDrive(u"drive-b"_s, toc);
    const QString discId = musicBrainzDiscId(toc).value();

    fixture.manager->setPreferredDrive(discId, u"drive-b"_s);
    auto resolved = fixture.manager->resolveDisc(discId, true);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(u"drive-b"_s, resolved->drive.id);

    second.error = CdError{.code = CdDriveError::NoMedia, .message = u"Removed"_s, .platformCode = 0};
    resolved     = fixture.manager->resolveDisc(discId, true);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(u"drive-a"_s, resolved->drive.id);
}

TEST(CdDriveManagerTest, ResolvesKnownDiscWithoutReprobingAfterDiscoveryCacheExpires)
{
    auto ownedBackend = std::make_unique<FakeBackend>();
    auto* backend     = ownedBackend.get();
    auto& drive       = backend->addDrive(u"drive-a"_s, makeToc());
    CdDriveManager manager{std::move(ownedBackend), std::chrono::milliseconds{0}};

    const auto observed = manager.observations(true);
    ASSERT_EQ(1, observed.size());
    ASSERT_FALSE(observed.front().discId.isEmpty());
    EXPECT_EQ(1, drive.openCalls);

    const auto resolved = manager.resolveDisc(observed.front().discId);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(u"drive-a"_s, resolved->drive.id);
    EXPECT_EQ(1, drive.openCalls);
}

TEST(CdDriveManagerTest, LeaseIsExclusiveAndReleasesWithRaii)
{
    ManagerFixture fixture;
    const CdToc toc = makeToc();
    fixture.backend->addDrive(u"drive-a"_s, toc);
    const QString discId = musicBrainzDiscId(toc).value();

    {
        auto lease = fixture.manager->acquire(discId);
        ASSERT_TRUE(lease.has_value()) << lease.error().message.toStdString();
        EXPECT_TRUE(static_cast<bool>(*lease));
        EXPECT_NE(nullptr, lease->session());
        EXPECT_EQ(discId, lease->discId());

        const auto busy = fixture.manager->acquire(discId);
        ASSERT_FALSE(busy.has_value());
        EXPECT_EQ(CdDriveError::DriveBusy, busy.error().code);
    }

    EXPECT_TRUE(fixture.manager->acquire(discId).has_value());
}

TEST(CdDriveManagerTest, LeaseBlocksObservationCdTextAndRefreshWithoutDiscardingCache)
{
    ManagerFixture fixture;
    auto& leasedDrive             = fixture.backend->addDrive(u"drive-a"_s, makeToc());
    leasedDrive.cdText.disc.title = u"Cached title"_s;
    auto& otherDrive              = fixture.backend->addDrive(u"drive-b"_s, makeToc(120, 240));

    const auto observed = fixture.manager->observations(true);
    const auto leasedObservation
        = std::ranges::find(observed, u"drive-a"_s, [](const auto& item) { return item.drive.id; });
    ASSERT_NE(observed.end(), leasedObservation);

    const auto cdText = fixture.manager->readCdText(*leasedObservation);
    ASSERT_TRUE(cdText.has_value());
    const uint64_t generation = leasedObservation->generation;

    auto lease = fixture.manager->acquire(leasedObservation->discId);
    ASSERT_TRUE(lease.has_value()) << lease.error().message.toStdString();
    const int leasedOpenCalls = leasedDrive.openCalls;
    const int otherOpenCalls  = otherDrive.openCalls;

    const auto busyObservation = fixture.manager->observeDrive(leasedObservation->drive);
    ASSERT_TRUE(busyObservation.error.has_value());
    EXPECT_EQ(CdDriveError::DriveBusy, busyObservation.error->code);
    EXPECT_EQ(leasedOpenCalls, leasedDrive.openCalls);

    const auto busyCdText = fixture.manager->readCdText(*leasedObservation);
    ASSERT_FALSE(busyCdText.has_value());
    EXPECT_EQ(CdDriveError::DriveBusy, busyCdText.error().code);
    EXPECT_EQ(leasedOpenCalls, leasedDrive.openCalls);

    const auto refreshed = fixture.manager->observations(true);
    const auto refreshedLeased
        = std::ranges::find(refreshed, u"drive-a"_s, [](const auto& item) { return item.drive.id; });
    ASSERT_NE(refreshed.end(), refreshedLeased);
    EXPECT_EQ(generation, refreshedLeased->generation);
    ASSERT_TRUE(refreshedLeased->cdText.has_value());
    EXPECT_EQ(u"Cached title"_s, refreshedLeased->cdText->disc.title);
    EXPECT_EQ(leasedOpenCalls, leasedDrive.openCalls);
    EXPECT_EQ(otherOpenCalls + 1, otherDrive.openCalls);
}

TEST(CdDriveManagerTest, SessionIsDestroyedBeforeLeaseReservationIsReleased)
{
    ManagerFixture fixture;
    auto& drive          = fixture.backend->addDrive(u"drive-a"_s, makeToc());
    const QString discId = musicBrainzDiscId(drive.toc).value();

    auto acquired = fixture.manager->acquire(discId);
    ASSERT_TRUE(acquired.has_value()) << acquired.error().message.toStdString();
    std::optional<CdDriveLease> lease{std::move(*acquired)};

    std::optional<CdError> destructionAcquireError;
    drive.sessionDestroyed = [&] {
        auto duringDestruction = fixture.manager->acquire(discId);
        if(!duringDestruction) {
            destructionAcquireError = duringDestruction.error();
        }
    };

    lease.reset();
    drive.sessionDestroyed = {};

    ASSERT_TRUE(destructionAcquireError.has_value());
    EXPECT_EQ(CdDriveError::DriveBusy, destructionAcquireError->code);
    EXPECT_TRUE(fixture.manager->acquire(discId).has_value());
}

TEST(CdDriveManagerTest, ConcurrentAcquisitionCreatesOnlyOneLeasePerDrive)
{
    ManagerFixture fixture;
    const CdToc toc = makeToc();
    fixture.backend->addDrive(u"drive-a"_s, toc);
    const QString discId = musicBrainzDiscId(toc).value();
    ASSERT_TRUE(fixture.manager->resolveDisc(discId, true).has_value());

    std::barrier start{3};
    std::barrier acquired{3};
    std::atomic_int successes{0};
    std::atomic_int busyFailures{0};

    auto acquire = [&] {
        start.arrive_and_wait();
        auto lease = fixture.manager->acquire(discId);
        if(lease) {
            ++successes;
        }
        else if(lease.error().code == CdDriveError::DriveBusy) {
            ++busyFailures;
        }
        acquired.arrive_and_wait();
    };

    std::jthread first{acquire};
    std::jthread second{acquire};
    start.arrive_and_wait();
    acquired.arrive_and_wait();

    EXPECT_EQ(1, successes.load());
    EXPECT_EQ(1, busyFailures.load());
}

TEST(CdDriveManagerTest, DifferentDrivesCanBeInspectedConcurrently)
{
    ManagerFixture fixture;
    auto& firstDrive                      = fixture.backend->addDrive(u"drive-a"_s, makeToc());
    auto& secondDrive                     = fixture.backend->addDrive(u"drive-b"_s, makeToc(120, 240));
    const std::vector<CdDriveInfo> drives = fixture.manager->drives();
    ASSERT_EQ(2, drives.size());

    std::barrier readsStarted{3};
    firstDrive.tocReadStarted = [&] {
        readsStarted.arrive_and_wait();
    };
    secondDrive.tocReadStarted = [&] {
        readsStarted.arrive_and_wait();
    };

    std::jthread first{[&] { [[maybe_unused]] const auto observed = fixture.manager->observeDrive(drives.at(0)); }};
    std::jthread second{[&] { [[maybe_unused]] const auto observed = fixture.manager->observeDrive(drives.at(1)); }};
    readsStarted.arrive_and_wait();
}

TEST(CdDriveManagerTest, LeaseCanOutliveManager)
{
    std::optional<CdDriveLease> lease;
    {
        ManagerFixture fixture;
        const CdToc toc = makeToc();
        fixture.backend->addDrive(u"drive-a"_s, toc);

        auto acquired = fixture.manager->acquire(musicBrainzDiscId(toc).value());
        ASSERT_TRUE(acquired.has_value()) << acquired.error().message.toStdString();
        lease.emplace(std::move(*acquired));
    }

    EXPECT_TRUE(static_cast<bool>(*lease));
    lease.reset();
}

TEST(CdDriveManagerTest, AcquireRevalidatesMediaAfterResolution)
{
    ManagerFixture fixture;
    const CdToc original = makeToc();
    auto& drive          = fixture.backend->addDrive(u"drive-a"_s, original);
    const QString discId = musicBrainzDiscId(original).value();

    ASSERT_TRUE(fixture.manager->resolveDisc(discId, true).has_value());
    drive.toc = makeToc(140, 260);

    const auto lease = fixture.manager->acquire(discId);
    ASSERT_FALSE(lease.has_value());
    EXPECT_EQ(CdDriveError::MediaChanged, lease.error().code);
}

TEST(CdDriveManagerTest, AcquireFallsBackWhenPreferredDriveLosesTheDisc)
{
    ManagerFixture fixture;
    const CdToc toc = makeToc();
    fixture.backend->addDrive(u"drive-a"_s, toc);
    auto& preferred      = fixture.backend->addDrive(u"drive-b"_s, toc);
    const QString discId = musicBrainzDiscId(toc).value();

    fixture.manager->setPreferredDrive(discId, u"drive-b"_s);
    ASSERT_EQ(u"drive-b"_s, fixture.manager->resolveDisc(discId, true)->drive.id);
    preferred.error = CdError{.code = CdDriveError::NoMedia, .message = u"Removed"_s, .platformCode = 0};

    auto lease = fixture.manager->acquire(discId);
    ASSERT_TRUE(lease.has_value()) << lease.error().message.toStdString();
    EXPECT_EQ(u"drive-a"_s, lease->drive().id);
}
} // namespace Fooyin::Cdda
