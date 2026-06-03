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

#include <QByteArray>
#include <QNetworkReply>

namespace Fooyin::StreamUtils {
struct BufferState
{
    QByteArray data;
    size_t maxBytes{0};
    quint64 bytesReceived{0};
    quint64 bytesRead{0};
    quint64 requestStartBytesReceived{0};

    void reset();
};

struct ReadModeState
{
    bool nonBlockingReadsEnabled{false};
    bool readWouldBlock{false};
    quint64 readWouldBlockCount{0};

    void reset();
};

size_t clampedBufferBytes(size_t maxBufferedBytes);
void appendReplyData(BufferState& streamBuffer, QNetworkReply* reply);
} // namespace Fooyin::StreamUtils
