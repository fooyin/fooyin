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

#include "cddrivebackend.h"

#include <cdio/cdio.h>
#include <cdio/paranoia/cdda.h>

namespace Fooyin::Cdda {
class LibcdioDriveBackend : public CdDriveBackend
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::Cdda::LibcdioDriveBackend)

public:
    std::vector<CdDriveInfo> drives() override;
    std::expected<OpenedDrive, CdError> open(const QString& driveId) override;
};

class LibcdioDriveSession : public CdDriveSession
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::Cdda::LibcdioDriveSession)

public:
    LibcdioDriveSession(CdIo_t* cdio, QString device);
    ~LibcdioDriveSession() override;

    std::expected<CdToc, CdError> readToc() override;
    CdText readCdText(const CdToc& toc) override;
    std::expected<CdSectorRead, CdError> readAudioSectors(int firstSector, int count) override;
    std::expected<CdSectorRead, CdError> readAudioSectorsSecure(int firstSector, int count,
                                                                CdRippingSecurity security) override;

    std::expected<void, CdError> setReadSpeed(int speed) override;

    QStringList takeWarnings() override;
    void cancel() override;

private:
    [[nodiscard]] CdIo_t* cdioHandle() const;
    std::expected<void, CdError> ensureParanoiaDrive();
    static CdError cancelledError();

    CdIo_t* m_cdio;
    cdrom_drive_t* m_drive;
    cdrom_paranoia_t* m_paranoia;
    std::optional<CdRippingSecurity> m_paranoiaSecurity;
    int m_nextParanoiaSector;
    QString m_device;
    QStringList m_warnings;
    std::atomic_bool m_cancelled;
};
} // namespace Fooyin::Cdda
