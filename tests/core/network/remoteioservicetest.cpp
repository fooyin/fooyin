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

#include <core/network/networkutils.h>
#include <core/network/remoteioservice.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QStandardPaths>

#include <algorithm>
#include <cstring>
#include <functional>
#include <optional>

using namespace Qt::StringLiterals;

namespace {
QCoreApplication* ensureCoreApplication()
{
    QStandardPaths::setTestModeEnabled(true);

    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    static int argc{1};
    static char appName[]  = "fooyin-remoteioservice-test";
    static char* argv[]    = {appName, nullptr};
    static auto* const app = new QCoreApplication{argc, argv};
    QCoreApplication::setApplicationName(QString::fromLatin1(appName));
    return app;
}

class FakeReply : public QNetworkReply
{
public:
    explicit FakeReply(const QNetworkRequest& request, QObject* parent = nullptr)
        : QNetworkReply{parent}
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        QIODevice::open(ReadOnly | Unbuffered);
    }

    void abort() override
    {
        m_aborted = true;
        setError(OperationCanceledError, u"aborted"_s);
        Q_EMIT finished();
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
    bool m_aborted{false};
};

class FakeNetworkAccessManager : public QNetworkAccessManager
{
public:
    QPointer<FakeReply> lastReply;
    QNetworkRequest lastRequest;

protected:
    QNetworkReply* createRequest(Operation /*operation*/, const QNetworkRequest& request,
                                 QIODevice* /*outgoingData*/) override
    {
        lastRequest = request;
        auto* reply = new FakeReply{request};
        lastReply   = reply;
        return reply;
    }
};

bool waitFor(const std::function<bool()>& condition)
{
    QElapsedTimer timer;
    timer.start();

    while(!condition() && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    return condition();
}
} // namespace

namespace Fooyin::Testing {
class RemoteIoServiceTest : public ::testing::Test
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

TEST_F(RemoteIoServiceTest, DownloadsUsingSharedNetworkManager)
{
    auto network = std::make_shared<FakeNetworkAccessManager>();
    RemoteIoService service{network, nullptr};

    std::optional<QByteArray> result;
    QString error;
    auto handle = service.download(QUrl{u"https://example.com/playlist.m3u"_s}, qApp,
                                   [&](std::optional<QByteArray> data, QString message) {
                                       result = std::move(data);
                                       error  = std::move(message);
                                   });
    ASSERT_NE(handle, nullptr);

    ASSERT_FALSE(network->lastReply.isNull());
    EXPECT_EQ(networkUserAgent(), network->lastRequest.rawHeader("User-Agent"));

    network->lastReply->finish("playlist-data");

    ASSERT_TRUE(waitFor([&] { return result.has_value(); }));
    EXPECT_EQ(QByteArray{"playlist-data"}, result.value());
    EXPECT_TRUE(error.isEmpty());
}

TEST_F(RemoteIoServiceTest, CancelsInFlightDownload)
{
    auto network = std::make_shared<FakeNetworkAccessManager>();
    RemoteIoService service{network, nullptr};

    bool finished{false};
    QString error;
    auto handle = service.download(QUrl{u"https://example.com/cover.jpg"_s}, qApp,
                                   [&](const std::optional<QByteArray>& data, QString message) {
                                       EXPECT_FALSE(data.has_value());
                                       error    = std::move(message);
                                       finished = true;
                                   });

    ASSERT_FALSE(network->lastReply.isNull());
    handle->cancel();

    ASSERT_TRUE(waitFor([&] { return finished; }));
    EXPECT_TRUE(network->lastReply.isNull() || network->lastReply->wasAborted());
    EXPECT_FALSE(error.isEmpty());
}

TEST_F(RemoteIoServiceTest, ReportsTimeoutExplicitly)
{
    auto network = std::make_shared<FakeNetworkAccessManager>();
    RemoteIoService service{network, nullptr};

    bool finished{false};
    QString error;
    auto handle = service.download(
        QUrl{u"https://example.com/slow.pls"_s}, qApp,
        [&](const std::optional<QByteArray>& data, QString message) {
            EXPECT_FALSE(data.has_value());
            error    = std::move(message);
            finished = true;
        },
        std::chrono::milliseconds{1});
    ASSERT_NE(handle, nullptr);

    ASSERT_TRUE(waitFor([&] { return finished; }));
    EXPECT_EQ(u"Timed out"_s, error);
}
} // namespace Fooyin::Testing
