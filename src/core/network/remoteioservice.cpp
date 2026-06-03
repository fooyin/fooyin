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

#include <core/network/remoteioservice.h>

#include "hlsstreamdevice.h"
#include "internalcoresettings.h"
#include "networkstreamdevice.h"

#include <core/network/networkutils.h>

#include <utils/settings/settingsmanager.h>

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <climits>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(REMOTE_IO, "fy.remoteio")

namespace Fooyin {
void RemoteDownloadHandle::cancel()
{
    m_cancelled.store(true, std::memory_order_release);

    QPointer<QNetworkReply> reply;
    {
        const std::scoped_lock lock{m_mutex};
        reply = m_reply;
    }

    if(reply) {
        QMetaObject::invokeMethod(reply, [reply] {
            if(reply) {
                reply->abort();
            }
        });
    }
}

bool RemoteDownloadHandle::isCancelled() const
{
    return m_cancelled.load(std::memory_order_acquire);
}

void RemoteDownloadHandle::setReply(QNetworkReply* reply)
{
    const std::scoped_lock lock{m_mutex};
    m_reply = reply;
}

void RemoteDownloadHandle::clearReply()
{
    const std::scoped_lock lock{m_mutex};
    m_reply = nullptr;
}

RemoteIoService::RemoteIoService(std::shared_ptr<QNetworkAccessManager> networkManager, SettingsManager* settings,
                                 QObject* parent)
    : QObject{parent}
    , m_networkManager{std::move(networkManager)}
    , m_settings{settings}
{ }

RemoteStreamSource RemoteIoService::createStreamSource(const QUrl& url) const
{
    const auto readAheadKb      = std::max(0, m_settings->value<Settings::Core::Internal::RemoteReadAheadKb>());
    const auto maxBufferedBytes = static_cast<qsizetype>(std::max(1, readAheadKb) * 1024);

    const QString path = url.path().toLower();

    if(path.endsWith(".m3u8"_L1)) {
        auto device = std::make_unique<HlsStreamDevice>(m_networkManager, url, maxBufferedBytes);

        RemoteStreamSource source;
        source.remoteDevice = device.get();
        source.device       = std::move(device);
        return source;
    }

    auto device = std::make_unique<NetworkStreamDevice>(m_networkManager, url, maxBufferedBytes);

    RemoteStreamSource source;
    source.remoteDevice = device.get();
    source.device       = std::move(device);
    return source;
}

std::shared_ptr<RemoteDownloadHandle> RemoteIoService::download(const QUrl& url, QObject* context,
                                                                DownloadCallback callback,
                                                                std::chrono::milliseconds timeout)
{
    auto handle = std::make_shared<RemoteDownloadHandle>();

    const auto timeoutMs = static_cast<int>(std::clamp(timeout, 1ms, std::chrono::milliseconds{INT_MAX}).count());

    const QPointer contextPtr{context};
    auto startRequest
        = [network = m_networkManager, url, timeoutMs, contextPtr, handle, callback = std::move(callback)]() mutable {
              if(handle->isCancelled()) {
                  return;
              }

              QNetworkRequest req = makeNetworkRequest(url);

              auto* reply = network->get(req);
              handle->setReply(reply);

              auto* timer = new QTimer(reply);
              timer->setSingleShot(true);
              auto timedOut = std::make_shared<std::atomic_bool>(false);

              QObject::connect(timer, &QTimer::timeout, reply, [reply, timedOut] {
                  timedOut->store(true, std::memory_order_release);
                  reply->abort();
              });

              QObject::connect(reply, &QNetworkReply::finished, reply,
                               [reply, contextPtr, handle, timedOut, callback = std::move(callback)]() mutable {
                                   handle->clearReply();

                                   std::optional<QByteArray> data;
                                   QString error;

                                   if(handle->isCancelled()) {
                                       error = u"Cancelled"_s;
                                   }
                                   else if(timedOut->load(std::memory_order_acquire)) {
                                       error = u"Timed out"_s;
                                   }
                                   else if(reply->error() == QNetworkReply::NoError) {
                                       data = reply->readAll();
                                   }
                                   else {
                                       error = reply->errorString();
                                   }

                                   reply->deleteLater();

                                   if(contextPtr) {
                                       QMetaObject::invokeMethod(contextPtr, [callback = std::move(callback),
                                                                              data     = std::move(data),
                                                                              error    = std::move(error)]() mutable {
                                           callback(std::move(data), std::move(error));
                                       });
                                   }
                               });

              timer->start(timeoutMs);
          };

    QMetaObject::invokeMethod(m_networkManager.get(), std::move(startRequest));
    return handle;
}
} // namespace Fooyin
