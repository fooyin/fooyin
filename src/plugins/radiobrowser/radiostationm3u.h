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
 *
 */

#pragma once

#include "radiostation.h"

#include <QString>

#include <vector>

namespace Fooyin::RadioBrowser {
struct RadioStationM3uEntry
{
    QString uuid;
    QString name;
    QString streamUrl;
};

struct RadioStationM3uReadResult
{
    std::vector<RadioStationM3uEntry> entries;
    QString error;
};

[[nodiscard]] RadioStationM3uReadResult readRadioStationM3u(const QString& filePath);
[[nodiscard]] bool writeRadioStationM3u(const QString& filePath, const RadioStationList& stations,
                                        QString* error = nullptr);
} // namespace Fooyin::RadioBrowser
