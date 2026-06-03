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

#include <core/network/remotesourceprovider.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class QIODevice;
class QNetworkAccessManager;
class QNetworkReply;

namespace Fooyin {
class SettingsManager;

class FYCORE_EXPORT RemoteDownloadHandle
{
public:
    void cancel();
    [[nodiscard]] bool isCancelled() const;

private:
    friend class RemoteIoService;

    void setReply(QNetworkReply* reply);
    void clearReply();

    std::atomic_bool m_cancelled{false};
    mutable std::mutex m_mutex;
    QPointer<QNetworkReply> m_reply;
};

class FYCORE_EXPORT RemoteIoService : public QObject,
                                      public RemoteSourceProvider
{
    Q_OBJECT

public:
    using DownloadCallback = std::function<void(std::optional<QByteArray> data, QString error)>;

    explicit RemoteIoService(std::shared_ptr<QNetworkAccessManager> networkManager, SettingsManager* settings,
                             QObject* parent = nullptr);

    [[nodiscard]] RemoteStreamSource createStreamSource(const QUrl& url) const override;

    [[nodiscard]] std::shared_ptr<RemoteDownloadHandle>
    download(const QUrl& url, QObject* context, DownloadCallback callback,
             std::chrono::milliseconds timeout = std::chrono::seconds{30});

private:
    std::shared_ptr<QNetworkAccessManager> m_networkManager;
    SettingsManager* m_settings;
};
} // namespace Fooyin
