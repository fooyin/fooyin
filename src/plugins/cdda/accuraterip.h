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

#include <core/engine/conversion/conversionrunner.h>

#include <QByteArray>
#include <QUrl>

#include <expected>
#include <mutex>
#include <optional>
#include <vector>

namespace Fooyin::Cdda {
struct AccurateRipDiscId
{
    uint32_t id1{0};
    uint32_t id2{0};
    uint32_t cddbId{0};
    int trackCount{0};

    bool operator==(const AccurateRipDiscId&) const = default;
};

struct AccurateRipChecksum
{
    uint32_t confidence{0};
    uint32_t crc{0};
    uint32_t crc2{0};
};
using AccurateRipPressing = std::vector<AccurateRipChecksum>;

struct AccurateRipDriveOffset
{
    QString name;
    int correctionSampleFrames{0};
    int submissions{0};
    int agreementPercent{0};
    bool purged{false};
};

enum class AccurateRipVerifyStatus : uint8_t
{
    Verified = 0,
    Mismatch,
    Incomplete,
    InvalidFormat,
};

struct AccurateRipTrackResult
{
    Track track;
    AccurateRipVerifyStatus status{AccurateRipVerifyStatus::Incomplete};
    uint32_t crcV1{0};
    uint32_t crcV2{0};
    int confidence{0};
    std::vector<uint32_t> databaseCrcs;
};

std::expected<AccurateRipDiscId, QString> accurateRipDiscId(const CdToc& toc);
QUrl accurateRipDiscUrl(const AccurateRipDiscId& id);

std::expected<std::vector<AccurateRipPressing>, QString> parseAccurateRipResponse(const QByteArray& data,
                                                                                  const AccurateRipDiscId& expected);
std::vector<AccurateRipDriveOffset> parseAccurateRipDriveOffsets(const QByteArray& html);
std::optional<AccurateRipDriveOffset> findAccurateRipDriveOffset(const std::vector<AccurateRipDriveOffset>& offsets,
                                                                 const QString& vendor, const QString& model);

class AccurateRipVerifier : public ConversionInputObserver
{
public:
    AccurateRipVerifier(CdToc toc, std::vector<AccurateRipPressing> pressings);
    ~AccurateRipVerifier() override;

    void trackStarted(const Track& track, const AudioFormat& format) override;
    void sourceAudio(const Track& track, const AudioBuffer& buffer) override;
    void trackFinished(const Track& track, bool complete) override;

    [[nodiscard]] std::vector<AccurateRipTrackResult> results() const;

private:
    struct State
    {
        Track track;
        uint64_t totalFrames{0};
        uint64_t position{0};
        uint32_t crcV1{0};
        uint32_t crcV2{0};
        bool validFormat{false};
        bool complete{false};
    };
    [[nodiscard]] State* stateFor(const Track& track);

    CdToc m_toc;
    std::vector<AccurateRipPressing> m_pressings;
    std::vector<State> m_states;
    mutable std::mutex m_mutex;
};
} // namespace Fooyin::Cdda
