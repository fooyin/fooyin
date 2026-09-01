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

#include <core/engine/audioloader.h>
#include <core/engine/conversion/conversionrunner.h>

#include <QByteArray>

#include <functional>
#include <memory>

namespace Fooyin {
enum class AudioVerificationStatus : uint8_t
{
    Succeeded = 0,
    Failed,
    Cancelled,
};

struct AudioVerificationResult
{
    Track track;
    AudioVerificationStatus status{AudioVerificationStatus::Failed};
    AudioFormat format;
    uint64_t decodedFrames{0};
    QByteArray md5;
    uint32_t crc32{0};
    QString error;
    QStringList warnings;
};

struct AudioVerificationProgress
{
    int trackIndex{0};
    int trackCount{0};
    Track track;
    uint64_t positionMs{0};
};

namespace AudioVerifier {
using ProgressCallback = std::function<void(const AudioVerificationProgress&)>;
using CancelCallback   = std::function<bool()>;

struct Request
{
    AudioLoader* audioLoader{nullptr};
    TrackList tracks;
    bool verifyIntegrity{true};
    ProgressCallback progressCallback;
    CancelCallback cancelCallback;
    std::shared_ptr<ConversionInputObserver> observer;
};

[[nodiscard]] FYCORE_EXPORT std::vector<AudioVerificationResult> run(const Request& request);
} // namespace AudioVerifier
} // namespace Fooyin
