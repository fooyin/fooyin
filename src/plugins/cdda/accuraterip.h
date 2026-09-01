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

#include <core/engine/verification/accuraterip.h>

#include <QByteArray>

#include <expected>
#include <optional>
#include <vector>

namespace Fooyin::Cdda {
struct AccurateRipDriveOffset
{
    QString name;
    int correctionSampleFrames{0};
    int submissions{0};
    int agreementPercent{0};
    bool purged{false};
};

std::expected<AccurateRip::DiscLayout, QString> accurateRipLayout(const CdToc& toc);
std::expected<AccurateRip::DiscId, QString> accurateRipDiscId(const CdToc& toc);
QUrl accurateRipDiscUrl(const AccurateRip::DiscId& id);

std::expected<std::vector<AccurateRip::Pressing>, QString>
parseAccurateRipResponse(const QByteArray& data, const AccurateRip::DiscId& expected);

std::vector<AccurateRipDriveOffset> parseAccurateRipDriveOffsets(const QByteArray& html);
std::optional<AccurateRipDriveOffset> findAccurateRipDriveOffset(const std::vector<AccurateRipDriveOffset>& offsets,
                                                                 const QString& vendor, const QString& model);
} // namespace Fooyin::Cdda
