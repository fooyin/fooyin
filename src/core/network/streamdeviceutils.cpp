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

#include "streamdeviceutils.h"

constexpr size_t MinimumBufferedBytes = 64UL * 1024;

namespace Fooyin::StreamUtils {
void BufferState::reset()
{
    data.clear();
    bytesReceived             = 0;
    bytesRead                 = 0;
    requestStartBytesReceived = 0;
}

void ReadModeState::reset()
{
    nonBlockingReadsEnabled = false;
    readWouldBlock          = false;
    readWouldBlockCount     = 0;
}

size_t clampedBufferBytes(size_t maxBufferedBytes)
{
    return std::max(MinimumBufferedBytes, maxBufferedBytes);
}

void appendReplyData(BufferState& streamBuffer, QNetworkReply* reply)
{
    const qsizetype availableCapacity = static_cast<qsizetype>(streamBuffer.maxBytes) - streamBuffer.data.size();
    if(availableCapacity <= 0) {
        return;
    }

    const QByteArray data = reply->read(availableCapacity);
    streamBuffer.bytesReceived += static_cast<quint64>(data.size());
    streamBuffer.data.append(data);
}
} // namespace Fooyin::StreamUtils
