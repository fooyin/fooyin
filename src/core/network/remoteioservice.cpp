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

#include <QBasicTimer>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>

#include <algorithm>
#include <climits>
#include <condition_variable>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(REMOTE_IO, "fy.remoteio")

namespace Fooyin {
struct PendingRemoteStreamState
{
    std::mutex mutex;
    std::condition_variable_any ready;
    std::shared_ptr<QNetworkAccessManager> network;
    std::stop_source cancellation;
    bool finished{false};
    QString error;
    RemoteStreamSource source;
    std::weak_ptr<PendingRemoteStream> handle;
};

namespace {
bool completePendingStream(const std::shared_ptr<PendingRemoteStreamState>& state, RemoteStreamSource source,
                           QString error)
{
    {
        const std::scoped_lock lock{state->mutex};
        if(state->finished) {
            return false;
        }
        state->finished = true;
        state->error    = std::move(error);
        state->source   = std::move(source);
    }
    state->ready.notify_all();

    QMetaObject::invokeMethod(
        state->network.get(),
        [weak = state->handle]() {
            if(const auto handle = weak.lock()) {
                Q_EMIT handle->finished();
            }
        },
        Qt::QueuedConnection);
    return true;
}

class RemoteStreamPreparation : public QObject
{
public:
    RemoteStreamPreparation(std::shared_ptr<PendingRemoteStreamState> state, RemoteStreamSource source)
        : QObject{state->network.get()}
        , m_state{std::move(state)}
        , m_source{std::move(source)}
    { }

    void start()
    {
        const QPointer guard{this};

        m_cancellation.emplace(m_state->cancellation.get_token(), [guard, network = m_state->network]() {
            QMetaObject::invokeMethod(
                network.get(),
                [guard]() {
                    if(guard) {
                        guard->finish(u"Cancelled"_s);
                    }
                },
                Qt::QueuedConnection);
        });

        if(m_state->cancellation.stop_requested()) {
            finish(u"Cancelled"_s);
            return;
        }
        if(!m_source.device || !m_source.device->open(QIODevice::ReadOnly)) {
            finish(m_source.device ? m_source.device->errorString() : u"No remote source available."_s);
            return;
        }

        m_timer.start(static_cast<int>(std::clamp<int64_t>(m_source.openTimeout.count(), 1, INT_MAX)), this);

        if(auto* device = qobject_cast<NetworkStreamDevice*>(m_source.device.get())) {
            device->setOpenCallback([guard](QString error) {
                if(guard) {
                    QMetaObject::invokeMethod(
                        guard,
                        [guard, error = std::move(error)]() mutable {
                            if(guard) {
                                guard->finish(std::move(error));
                            }
                        },
                        Qt::QueuedConnection);
                }
            });
        }
        else {
            // HLS devices have fixed sequential access that is independent of their response headers
            finish({});
        }
    }

protected:
    void timerEvent(QTimerEvent* event) override
    {
        if(event->timerId() == m_timer.timerId()) {
            finish(u"Timed out preparing remote stream."_s);
            return;
        }
        QObject::timerEvent(event);
    }

private:
    void finish(QString error)
    {
        if(std::exchange(m_finishing, true)) {
            return;
        }

        m_timer.stop();
        m_cancellation.reset();

        if(error.isEmpty()) {
            // Detach on the network thread so takeSource() can adopt the device on its consumer's thread
            m_source.device->moveToThread(nullptr);
            completePendingStream(m_state, std::move(m_source), {});
        }
        else {
            completePendingStream(m_state, {}, std::move(error));
            if(m_source.device) {
                m_source.device->close();
            }
        }

        deleteLater();
    }

    std::shared_ptr<PendingRemoteStreamState> m_state;
    RemoteStreamSource m_source;
    QBasicTimer m_timer;
    std::optional<std::stop_callback<std::function<void()>>> m_cancellation;
    bool m_finishing{false};
};
} // namespace

PendingRemoteStream::~PendingRemoteStream()
{
    cancel();
}

void PendingRemoteStream::cancel()
{
    if(completePendingStream(m_state, {}, u"Cancelled"_s)) {
        m_state->cancellation.request_stop();
    }
}

bool PendingRemoteStream::isFinished() const
{
    const std::scoped_lock lock{m_state->mutex};
    return m_state->finished;
}

bool PendingRemoteStream::isReady() const
{
    const std::scoped_lock lock{m_state->mutex};
    return m_state->finished && m_state->error.isEmpty();
}

QString PendingRemoteStream::errorString() const
{
    const std::scoped_lock lock{m_state->mutex};
    return m_state->error;
}

bool PendingRemoteStream::waitForReady(std::chrono::milliseconds timeout, std::stop_token token)
{
    std::unique_lock lock{m_state->mutex};
    if(!m_state->finished && QThread::currentThread() == m_state->network->thread()) {
        return false;
    }

    m_state->ready.wait_for(lock, token, std::max(0ms, timeout), [state = m_state]() { return state->finished; });
    if(m_state->finished) {
        return m_state->error.isEmpty();
    }
    lock.unlock();

    if(completePendingStream(m_state, {},
                             token.stop_requested() ? u"Cancelled"_s : u"Timed out preparing remote stream."_s)) {
        m_state->cancellation.request_stop();
    }
    return isReady();
}

RemoteStreamSource PendingRemoteStream::takeSource()
{
    const std::scoped_lock lock{m_state->mutex};
    if(!m_state->finished || !m_state->error.isEmpty() || !m_state->source.device) {
        return {};
    }

    auto source                  = std::move(m_state->source);
    m_state->source.remoteDevice = nullptr;
    source.device->moveToThread(QThread::currentThread());
    return source;
}

PendingRemoteStream::PendingRemoteStream(std::shared_ptr<PendingRemoteStreamState> state)
    : m_state{std::move(state)}
{
    moveToThread(m_state->network->thread());
}

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
    const auto openTimeoutMs    = std::max(1000, m_settings->value<Settings::Core::Internal::RemoteOpenTimeoutMs>());

    if(isHlsStreamUrl(url)) {
        auto device = std::make_unique<HlsStreamDevice>(m_networkManager, url, maxBufferedBytes);

        RemoteStreamSource source;
        source.remoteDevice = device.get();
        source.openTimeout  = std::chrono::milliseconds{openTimeoutMs};
        source.device       = std::move(device);
        return source;
    }

    auto device = std::make_unique<NetworkStreamDevice>(m_networkManager, url, maxBufferedBytes);
    device->setOpenTimeout(std::chrono::milliseconds{openTimeoutMs});

    RemoteStreamSource source;
    source.remoteDevice = device.get();
    source.openTimeout  = std::chrono::milliseconds{openTimeoutMs};
    source.device       = std::move(device);
    return source;
}

std::shared_ptr<PendingRemoteStream> RemoteIoService::prepareStream(const QUrl& url) const
{
    auto state     = std::make_shared<PendingRemoteStreamState>();
    state->network = m_networkManager;

    auto pending
        = std::shared_ptr<PendingRemoteStream>{new PendingRemoteStream{state}, [](PendingRemoteStream* handle) {
                                                   handle->cancel();
                                                   if(QThread::currentThread() == handle->thread()) {
                                                       delete handle;
                                                   }
                                                   else {
                                                       handle->deleteLater();
                                                   }
                                               }};
    state->handle = pending;

    auto source = createStreamSource(url);
    source.device->moveToThread(m_networkManager->thread());

    const bool invoked = QMetaObject::invokeMethod(
        m_networkManager.get(),
        [state, source = std::move(source)]() mutable {
            auto* preparation = new RemoteStreamPreparation{state, std::move(source)};
            preparation->start();
        },
        Qt::QueuedConnection);
    if(!invoked) {
        completePendingStream(state, {}, u"Failed to queue remote stream preparation."_s);
    }

    return pending;
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

              const QNetworkRequest req = makeNetworkRequest(url);

              auto* reply = network->get(req);
              handle->setReply(reply);

              auto* timer = new QTimer(reply);
              timer->setSingleShot(true);
              const auto timedOut = std::make_shared<std::atomic_bool>(false);

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
