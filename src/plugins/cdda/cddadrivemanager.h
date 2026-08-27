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

#pragma once

#include "drive/cddrivebackend.h"

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include <QCoreApplication>

namespace Fooyin::Cdda {
struct CdDriveAccessState;

struct CdDriveObservation
{
    CdDriveInfo drive;
    std::optional<CdToc> toc;
    std::optional<CdText> cdText;
    QString discId;
    std::optional<CdError> error;
    uint64_t generation{0};

    bool operator==(const CdDriveObservation&) const = default;
};

class CdDriveLease
{
public:
    CdDriveLease() = default;
    ~CdDriveLease();

    CdDriveLease(const CdDriveLease&)            = delete;
    CdDriveLease& operator=(const CdDriveLease&) = delete;
    CdDriveLease(CdDriveLease&& other) noexcept;
    CdDriveLease& operator=(CdDriveLease&& other) noexcept;

    [[nodiscard]] explicit operator bool() const;

    [[nodiscard]] CdDriveSession* session() const;
    [[nodiscard]] const CdDriveInfo& drive() const;
    [[nodiscard]] const CdToc& toc() const;
    [[nodiscard]] const QString& discId() const;
    [[nodiscard]] uint64_t generation() const;

private:
    friend class CdDriveManager;

    CdDriveLease(std::shared_ptr<CdDriveAccessState> state, QString driveId, CdDriveInfo drive, CdToc toc,
                 QString discId, uint64_t generation, std::unique_ptr<CdDriveSession> session);
    void release();

    std::shared_ptr<CdDriveAccessState> m_state;
    QString m_driveId;
    CdDriveInfo m_drive;
    CdToc m_toc;
    QString m_discId;
    uint64_t m_generation{0};
    std::unique_ptr<CdDriveSession> m_session;
};

class CdDriveManagerPrivate;

class CdDriveManager
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::Cdda::CdDriveManager)

public:
    explicit CdDriveManager(std::unique_ptr<CdDriveBackend> backend,
                            std::chrono::milliseconds cacheLifetime = std::chrono::seconds{2});
    ~CdDriveManager();

    CdDriveManager(const CdDriveManager&)            = delete;
    CdDriveManager& operator=(const CdDriveManager&) = delete;

    //! Enumerates drive devices without opening them or reading media.
    [[nodiscard]] std::vector<CdDriveInfo> drives();
    //! Opens and inspects one selected drive, updating its cache; returns DriveBusy while the drive is leased.
    [[nodiscard]] CdDriveObservation observeDrive(const CdDriveInfo& drive);
    //! Reads optional CD-Text for an already observed disc and caches it; returns DriveBusy for a leased drive.
    [[nodiscard]] std::expected<CdText, CdError> readCdText(const CdDriveObservation& observation);
    //! Returns cached observations until their lifetime expires; refreshes retain cached data for leased drives.
    [[nodiscard]] std::vector<CdDriveObservation> observations(bool forceRefresh = false);
    //! Resolves a stable disc identity to an inserted drive, preferring the user's current selection.
    [[nodiscard]] std::expected<CdDriveObservation, CdError> resolveDisc(const QString& discId,
                                                                         bool forceRefresh = false);
    //! Reopens and revalidates the selected disc while reserving all physical I/O to that drive.
    [[nodiscard]] std::expected<CdDriveLease, CdError> acquire(const QString& discId);

    void setPreferredDrive(const QString& discId, const QString& driveId);
    void invalidateDrive(const QString& driveId);
    void invalidateAll();

private:
    std::unique_ptr<CdDriveManagerPrivate> p;
};
} // namespace Fooyin::Cdda
