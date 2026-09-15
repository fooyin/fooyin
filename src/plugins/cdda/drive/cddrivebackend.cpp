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

#include "cddrivebackend.h"

#include <QLoggingCategory>

#include <limits>

Q_LOGGING_CATEGORY(CDDA_DRIVE_BACKEND, "fy.cdda.drivebackend")

namespace Fooyin::Cdda {
CdText CdDriveSession::readCdText(const CdToc& /*toc*/)
{
    return {};
}

std::expected<CdSectorRead, CdError> CdDriveSession::readAudioSectorsSecure(int firstSector, int count,
                                                                            CdRippingSecurity security)
{
    if(security == CdRippingSecurity::Disabled) {
        return readAudioSectors(firstSector, count);
    }
    return std::unexpected(CdError{.code         = CdDriveError::Unsupported,
                                   .message      = tr("Secure CD extraction is unavailable for this drive backend"),
                                   .platformCode = 0});
}

std::expected<void, CdError> CdDriveSession::setReadSpeed(int /*speed*/)
{
    return std::unexpected(CdError{.code         = CdDriveError::Unsupported,
                                   .message      = tr("CD read speed control is unavailable for this drive backend"),
                                   .platformCode = 0});
}

QStringList CdDriveSession::takeWarnings()
{
    return {};
}

std::expected<void, CdError> validateSectorRead(const CdSectorRead& read, int requestedSectors)
{
    if(requestedSectors < 0 || read.sectorsRead < 0 || read.sectorsRead > requestedSectors) {
        qCWarning(CDDA_DRIVE_BACKEND) << "CD drive returned an invalid sector count:"
                                      << "requested=" << requestedSectors << "returned=" << read.sectorsRead;
        return std::unexpected(CdError{.code = CdDriveError::ReadFailed, .message = {}, .platformCode = 0});
    }

    const auto expectedBytes = static_cast<qsizetype>(read.sectorsRead) * BytesPerSector;
    if(read.pcm.size() != expectedBytes) {
        qCWarning(CDDA_DRIVE_BACKEND) << "CD drive returned an invalid sector buffer:"
                                      << "sectors=" << read.sectorsRead << "actualBytes=" << read.pcm.size()
                                      << "expectedBytes=" << expectedBytes;
        return std::unexpected(CdError{.code = CdDriveError::ReadFailed, .message = {}, .platformCode = 0});
    }

    return {};
}

std::expected<void, CdError> validateSectorReadRequest(int firstSector, int count, int maximumSectors)
{
    if(firstSector < 0 || count < 0 || maximumSectors <= 0 || count > maximumSectors
       || firstSector > std::numeric_limits<int>::max() - count) {
        qCWarning(CDDA_DRIVE_BACKEND) << "Rejected invalid backend sector request:"
                                      << "firstSector=" << firstSector << "count=" << count
                                      << "maximum=" << maximumSectors;
        return std::unexpected(CdError{.code = CdDriveError::ReadFailed, .message = {}, .platformCode = EINVAL});
    }

    return {};
}
} // namespace Fooyin::Cdda
