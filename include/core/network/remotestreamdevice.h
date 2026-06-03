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

#include <QString>

#include <chrono>
#include <optional>

namespace Fooyin {
struct NetworkStreamMetadata
{
    QString streamName;
    QString streamGenre;
    QString streamTitle;
    QString streamUrl;
    int bitrateKbps{0};
    quint64 revision{0};
};

class FYCORE_EXPORT RemoteStreamDevice
{
public:
    static constexpr auto ReadWaitTimeout = std::chrono::milliseconds{50};

    virtual ~RemoteStreamDevice() = default;

    [[nodiscard]] virtual bool readWouldBlock() const         = 0;
    [[nodiscard]] virtual qsizetype bufferedByteCount() const = 0;
    virtual void setNonBlockingReadsEnabled(bool enabled)     = 0;
    virtual void setReconnectOnFinishedEnabled(bool enabled);

    [[nodiscard]] virtual bool shouldExposePathToDecoder() const;
    [[nodiscard]] virtual std::optional<NetworkStreamMetadata> remoteStreamMetadata() const;
};
} // namespace Fooyin
