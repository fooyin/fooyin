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

#include "cddatypes.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <expected>
#include <memory>

namespace Fooyin::Cdda {
enum class CdRippingSecurity : uint8_t
{
    Disabled = 0,
    Standard,
    Paranoid,
};

struct CdDriveInfo
{
    QString id; // Backend identifier passed back to CdDriveBackend::open()
    QString displayName;
    QString settingsKey;
    bool supportsSpeedLimit{false};
    QString vendor;
    QString model;
    QString revision;

    bool operator==(const CdDriveInfo&) const = default;
};

enum class CdDriveError : uint8_t
{
    None = 0,
    NoMedia,
    TrayOpen,
    AccessDenied,
    NotAudioDisc,
    MediaChanged,
    DriveBusy,
    ReadFailed,
    Unsupported,
    Cancelled,
};

struct CdError
{
    CdDriveError code{CdDriveError::None};
    QString message;
    int platformCode{0};

    bool operator==(const CdError&) const = default;
};

struct CdSectorRead
{
    QByteArray pcm;
    int sectorsRead{0};

    bool operator==(const CdSectorRead&) const = default;
};

class CdDriveSession
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::Cdda::CdDriveSession)

public:
    virtual ~CdDriveSession() = default;

    virtual std::expected<CdToc, CdError> readToc() = 0;
    virtual CdText readCdText(const CdToc& toc);
    virtual std::expected<CdSectorRead, CdError> readAudioSectors(int firstSector, int count) = 0;
    /*! Reads sectors using the requested extraction policy.
     *  Backends without an error-correcting reader support Disabled only.
     */
    virtual std::expected<CdSectorRead, CdError> readAudioSectorsSecure(int firstSector, int count,
                                                                        CdRippingSecurity security);

    //! Limits reads to the requested CD speed multiplier. Zero selects the drive's maximum speed.
    virtual std::expected<void, CdError> setReadSpeed(int speed);

    virtual QStringList takeWarnings();
    //! Requests cancellation of an active read; safe to call from another thread.
    virtual void cancel() = 0;
};

class CdDriveBackend
{
public:
    virtual ~CdDriveBackend() = default;

    struct OpenedDrive
    {
        CdDriveInfo drive;
        std::unique_ptr<CdDriveSession> session;
    };

    //! Enumerates stable device identifiers without opening or probing the drives.
    virtual std::vector<CdDriveInfo> drives() = 0;
    //! Opens a drive and returns hardware information obtained from the reserved handle.
    virtual std::expected<OpenedDrive, CdError> open(const QString& driveId) = 0;
};

std::expected<void, CdError> validateSectorRead(const CdSectorRead& read, int requestedSectors);
std::expected<void, CdError> validateSectorReadRequest(int firstSector, int count, int maximumSectors);
} // namespace Fooyin::Cdda
