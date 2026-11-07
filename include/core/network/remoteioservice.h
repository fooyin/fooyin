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
#include <stop_token>

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
struct PendingRemoteStreamState;

/*!
 * Prepares a remote source without exposing its device until its access capabilities are known.
 * The handle lives on the network thread and is owned by the returned shared pointer. Do not give it
 * a QObject parent or move it to another thread. Dropping the last handle cancels pending preparation
 * and arranges destruction on the network thread, releasing any untaken source.
 *
 * The methods below may be called from any thread. Completion state and blocking waits do not depend
 * on signal delivery. When connecting after preparation has begun, also check isFinished() and handle
 * completion only once: signals are not re-emitted for late connections.
 */
class FYCORE_EXPORT PendingRemoteStream : public QObject
{
    Q_OBJECT

public:
    ~PendingRemoteStream() override;

    PendingRemoteStream(const PendingRemoteStream&)            = delete;
    PendingRemoteStream& operator=(const PendingRemoteStream&) = delete;

    //! Cancel pending preparation. Has no effect after completion.
    void cancel();
    [[nodiscard]] bool isFinished() const;
    //! True when preparation succeeded, including after the source has been taken.
    [[nodiscard]] bool isReady() const;
    [[nodiscard]] QString errorString() const;

    /*!
     * Wait for preparation on a worker thread. Timeout or cancellation ends preparation.
     * Returns immediately without cancelling if called on the network thread while still pending.
     */
    bool waitForReady(std::chrono::milliseconds timeout, std::stop_token token = {});

    /*!
     * Take the opened source once in the thread that will use it. Returns an empty source before
     * successful completion or after it has already been taken. Readiness doesn't guarantee body data.
     */
    RemoteStreamSource takeSource();

Q_SIGNALS:
    //! Emitted once on the network thread after success, failure, cancellation or timeout.
    void finished();

private:
    friend class RemoteIoService;
    explicit PendingRemoteStream(std::shared_ptr<PendingRemoteStreamState> state);

    std::shared_ptr<PendingRemoteStreamState> m_state;
};

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

    //! Create an unopened source. Prefer prepareStream() when probing could precede the HTTP response headers.
    [[nodiscard]] RemoteStreamSource createStreamSource(const QUrl& url) const override;

    //! Begin asynchronous preparation using the configured remote open timeout.
    [[nodiscard]] std::shared_ptr<PendingRemoteStream> prepareStream(const QUrl& url) const;

    [[nodiscard]] std::shared_ptr<RemoteDownloadHandle>
    download(const QUrl& url, QObject* context, DownloadCallback callback,
             std::chrono::milliseconds timeout = std::chrono::seconds{30});

private:
    std::shared_ptr<QNetworkAccessManager> m_networkManager;
    SettingsManager* m_settings;
};
} // namespace Fooyin
