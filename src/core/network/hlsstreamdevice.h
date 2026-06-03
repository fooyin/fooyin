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

#include <core/network/remotestreamdevice.h>

#include <QIODevice>
#include <QNetworkAccessManager>
#include <QUrl>

#include <memory>

namespace Fooyin {
class HlsStreamDeviceState;

class FYCORE_EXPORT HlsStreamDevice : public QIODevice,
                                      public RemoteStreamDevice
{
public:
    explicit HlsStreamDevice(std::shared_ptr<QNetworkAccessManager> network, QUrl url, qsizetype maxBufferedBytes,
                             QObject* parent = nullptr);
    ~HlsStreamDevice() override;

    [[nodiscard]] bool isSequential() const override;
    [[nodiscard]] bool readWouldBlock() const override;
    [[nodiscard]] qsizetype bufferedByteCount() const override;
    [[nodiscard]] bool shouldExposePathToDecoder() const override;
    void setNonBlockingReadsEnabled(bool enabled) override;

    bool open(OpenMode mode) override;
    void close() override;

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    std::shared_ptr<QNetworkAccessManager> m_network;

    QUrl m_url;
    std::shared_ptr<HlsStreamDeviceState> m_state;
};
} // namespace Fooyin
