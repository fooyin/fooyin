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

#include <core/engine/audioencoderregistry.h>
#include <core/engine/audioloader.h>
#include <core/engine/conversion/conversionpathresolver.h>

#include <QStringList>

#include <functional>

namespace Fooyin {
class DspRegistry;

enum class ConversionResultStatus : uint8_t
{
    Succeeded = 0,
    Skipped,
    Failed,
    Cancelled,
};

struct ConversionTrackResult
{
    Track sourceTrack;
    QString outputPath;
    ConversionResultStatus status{ConversionResultStatus::Failed};
    QString error;
    QStringList warnings;
};

struct ConversionProgress
{
    int trackIndex{0};
    int trackCount{0};
    QString sourcePath;
    QString outputPath;
    uint64_t sourcePositionMs{0};
    uint64_t sourceDurationMs{0};
};

namespace ConversionRunner {
using ProgressCallback     = std::function<void(const ConversionProgress&)>;
using CancelCallback       = std::function<bool()>;
using ExistingFileCallback = std::function<ExistingFileMode(const QString&)>;

struct Request
{
    AudioLoader* audioLoader{nullptr};
    AudioEncoderRegistry* encoderRegistry{nullptr};
    DspRegistry* dspRegistry{nullptr};
    ConversionJob job;
    QString askFolder;
    ProgressCallback progressCallback;
    CancelCallback cancelCallback;
    ExistingFileCallback existingFileCallback;
};

FYCORE_EXPORT std::vector<ConversionTrackResult> run(const Request& request);
}; // namespace ConversionRunner
} // namespace Fooyin
