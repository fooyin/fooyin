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

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>

#include <cstring>
#include <memory>

namespace Fooyin::Testing {
class FakeNetworkReply : public QNetworkReply
{
public:
    FakeNetworkReply(const QNetworkRequest& request, QNetworkAccessManager::Operation operation, bool finishOnAbort,
                     QObject* parent = nullptr)
        : QNetworkReply{parent}
        , m_finishOnAbort{finishOnAbort}
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        QIODevice::open(ReadOnly | Unbuffered);
    }

    void abort() override
    {
        m_aborted = true;
        if(m_finishOnAbort) {
            setError(OperationCanceledError, QStringLiteral("aborted"));
            Q_EMIT finished();
        }
    }

    [[nodiscard]] bool wasAborted() const
    {
        return m_aborted;
    }

    [[nodiscard]] bool isSequential() const override
    {
        return true;
    }

    [[nodiscard]] qint64 bytesAvailable() const override
    {
        return m_data.size() + QNetworkReply::bytesAvailable();
    }

    void appendData(const QByteArray& data)
    {
        m_data.append(data);
    }

    void emitReadyRead()
    {
        Q_EMIT readyRead();
    }

    void setResponseHeader(const QByteArray& name, const QByteArray& value)
    {
        setRawHeader(name, value);
    }

    void emitFinished()
    {
        Q_EMIT finished();
    }

    void finish(QByteArray data)
    {
        m_data = std::move(data);
        Q_EMIT readyRead();
        Q_EMIT finished();
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        const qint64 bytesRead = std::min<qint64>(m_data.size(), maxSize);
        if(bytesRead <= 0) {
            return 0;
        }

        std::memcpy(data, m_data.constData(), bytesRead);
        m_data.remove(0, bytesRead);
        return bytesRead;
    }

private:
    QByteArray m_data;
    bool m_finishOnAbort;
    bool m_aborted{false};
};

class FakeNetworkAccessManager : public QNetworkAccessManager
{
public:
    explicit FakeNetworkAccessManager(bool finishRepliesOnAbort = false)
        : m_finishRepliesOnAbort{finishRepliesOnAbort}
    { }

    QPointer<FakeNetworkReply> lastReply;
    QNetworkRequest lastRequest;
    int requestCount{0};

protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                 QIODevice* /*outgoingData*/) override
    {
        ++requestCount;
        lastRequest = request;
        auto* reply = new FakeNetworkReply{request, operation, m_finishRepliesOnAbort};
        lastReply   = reply;
        return reply;
    }

private:
    bool m_finishRepliesOnAbort;
};

inline std::shared_ptr<FakeNetworkAccessManager> makeFakeNetworkAccessManager(bool finishRepliesOnAbort = false)
{
    return {new FakeNetworkAccessManager{finishRepliesOnAbort},
            [](FakeNetworkAccessManager* manager) { manager->deleteLater(); }};
}
} // namespace Fooyin::Testing
