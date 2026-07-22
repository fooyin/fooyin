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

#include "fycore_export.h"

#include <core/engine/conversion/conversiondefs.h>

#include <QCoreApplication>
#include <QString>

#include <vector>

namespace Fooyin {
enum class ConversionPathStatus : uint8_t
{
    Ready = 0,
    Skipped,
    NeedsOverwriteDecision,
    Error,
};

struct ConversionPathResult
{
    Track track;
    TrackList tracks;
    QString outputPath;
    ConversionPathStatus status{ConversionPathStatus::Error};
    QString error;
};

class FYCORE_EXPORT ConversionPathResolver
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::ConversionPathResolver)

public:
    struct Request
    {
        TrackList tracks;
        ConversionDestination destination;
        QString extension;
        QString askFolder;
    };

    static std::vector<ConversionPathResult> resolve(const Request& request);
    static QString previewPath(const Track& track, const ConversionDestination& destination, const QString& extension,
                               const QString& askFolder = {});
};
} // namespace Fooyin
