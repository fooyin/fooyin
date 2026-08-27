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

#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>

namespace Fooyin::Cdda {
constexpr auto MaximumSectorsPerPolicyRead = 16;
constexpr auto MaximumFramesPerPolicyRead  = MaximumSectorsPerPolicyRead * FramesPerSector;

struct CdFrameRead
{
    QByteArray pcm; // Interleaved signed 16-bit stereo PCM.
    int framesRead{0};

    bool operator==(const CdFrameRead&) const = default;
};

class CddaSectorReader
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::Cdda::CddaSectorReader)

public:
    CddaSectorReader(CdDriveSession* session, CdRippingSecurity security, std::stop_token stopToken = {});

    [[nodiscard]] std::expected<CdSectorRead, CdError> readAudioSectors(int firstSector, int count);
    [[nodiscard]] std::expected<CdFrameRead, CdError> readCorrectedFrames(const CdToc& toc, const CdTocTrack& track,
                                                                          uint64_t firstFrame, int frameCount,
                                                                          int readOffsetFrames);
    [[nodiscard]] QStringList takeWarnings();

private:
    [[nodiscard]] std::expected<CdSectorRead, CdError> readExact(int firstSector, int count);
    [[nodiscard]] std::optional<CdError> cancellationError() const;

    CdDriveSession* m_session;
    CdRippingSecurity m_security;
    std::stop_token m_stopToken;
    QStringList m_warnings;
};
} // namespace Fooyin::Cdda
