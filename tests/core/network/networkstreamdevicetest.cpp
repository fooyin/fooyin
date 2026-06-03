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

#include <core/network/networkstreamdevice.h>
#include <core/network/networkutils.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QNetworkReply>
#include <QPointer>
#include <QStandardPaths>

#include <algorithm>
#include <cstring>
#include <utility>

using namespace Qt::StringLiterals;

namespace {
QCoreApplication* ensureCoreApplication()
{
    QStandardPaths::setTestModeEnabled(true);

    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    // Keep the test application alive until process exit
    // Qt Network can keep shutdown work queued after the fake replies have been destroyed
    static int argc{1};
    static char appName[]  = "fooyin-networkstreamdevice-test";
    static char* argv[]    = {appName, nullptr};
    static auto* const app = new QCoreApplication{argc, argv};
    QCoreApplication::setApplicationName(QString::fromLatin1(appName));
    return app;
}

class FakeReply : public QNetworkReply
{
public:
    FakeReply(const QNetworkRequest& request, QNetworkAccessManager::Operation operation, QObject* parent = nullptr)
        : QNetworkReply{parent}
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        QIODevice::open(ReadOnly | Unbuffered);
    }

    void abort() override
    {
        m_aborted = true;
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

    void emitFinished()
    {
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
    bool m_aborted{false};
};

class FakeNetworkAccessManager : public QNetworkAccessManager
{
public:
    QPointer<FakeReply> lastReply;
    QNetworkRequest lastRequest;
    int requestCount{0};

protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                 QIODevice* /*outgoingData*/) override
    {
        ++requestCount;
        lastRequest = request;
        auto* reply = new FakeReply{request, operation};
        lastReply   = reply;
        return reply;
    }
};

std::shared_ptr<FakeNetworkAccessManager> makeFakeNetworkAccessManager()
{
    return {new FakeNetworkAccessManager{}, [](FakeNetworkAccessManager* manager) { manager->deleteLater(); }};
}
} // namespace

namespace Fooyin::Testing {
class NetworkStreamDeviceTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ensureCoreApplication();
    }

    void TearDown() override
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

TEST_F(NetworkStreamDeviceTest, SendsStreamRequestHeaders)
{
    auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    EXPECT_EQ(QByteArray{"*/*"}, network->lastRequest.rawHeader("Accept"));
    EXPECT_EQ(networkUserAgent(), network->lastRequest.rawHeader("User-Agent"));
    EXPECT_EQ(QByteArray{"1"}, network->lastRequest.rawHeader("Icy-MetaData"));
}

TEST_F(NetworkStreamDeviceTest, FailsReadsOnNetworkThread)
{
    auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));

    char data{0};
    EXPECT_LT(device.read(&data, 1), 0);
    EXPECT_FALSE(device.errorString().isEmpty());
}

TEST_F(NetworkStreamDeviceTest, LateReplySignalsAfterDeviceDestructionDoNotUseDestroyedDevice)
{
    auto network = makeFakeNetworkAccessManager();
    QPointer<FakeReply> reply;

    {
        NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};
        ASSERT_TRUE(device.open(QIODevice::ReadOnly));
        reply = network->lastReply;
        ASSERT_FALSE(reply.isNull());
    }

    ASSERT_FALSE(reply.isNull());
    EXPECT_TRUE(reply->wasAborted());

    reply->appendData("late-data");
    reply->emitReadyRead();
    reply->emitFinished();

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(reply.isNull());
}

TEST_F(NetworkStreamDeviceTest, ReconnectsAfterCleanFinishWhenEnabled)
{
    auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    device.setNonBlockingReadsEnabled(true);
    device.setReconnectOnFinishedEnabled(true);

    QPointer<FakeReply> firstReply = network->lastReply;
    ASSERT_FALSE(firstReply.isNull());

    firstReply->emitFinished();
    QCoreApplication::processEvents();

    QPointer<FakeReply> secondReply = network->lastReply;
    ASSERT_FALSE(secondReply.isNull());
    EXPECT_NE(firstReply, secondReply);
}

TEST_F(NetworkStreamDeviceTest, StopsReconnectLoopAfterRepeatedEmptyCleanFinishes)
{
    auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    device.setNonBlockingReadsEnabled(true);
    device.setReconnectOnFinishedEnabled(true);

    static constexpr int MaxReconnects = 5;
    ASSERT_EQ(network->requestCount, 1);

    for(int i{0}; i < MaxReconnects; ++i) {
        QPointer<FakeReply> reply = network->lastReply;
        ASSERT_FALSE(reply.isNull());

        reply->emitFinished();
        QCoreApplication::processEvents();

        ASSERT_EQ(network->requestCount, i + 2);
        ASSERT_FALSE(network->lastReply.isNull());
        EXPECT_NE(reply, network->lastReply);
    }

    QPointer<FakeReply> finalReply = network->lastReply;
    ASSERT_FALSE(finalReply.isNull());
    finalReply->emitFinished();
    QCoreApplication::processEvents();

    EXPECT_EQ(network->requestCount, MaxReconnects + 1);
}
} // namespace Fooyin::Testing
