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

#include "networktestutils.h"

#include <core/internalcoresettings.h>
#include <core/network/networkutils.h>
#include <core/network/remoteioservice.h>
#include <core/network/remotestreamdevice.h>
#include <utils/settings/settingsmanager.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <stop_token>

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

bool waitFor(const std::function<bool()>& condition)
{
    QElapsedTimer timer;
    timer.start();

    while(!condition() && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    return condition();
}
} // namespace

namespace Fooyin::Testing {
class RemoteIoServiceTest : public ::testing::Test
{
protected:
    RemoteIoServiceTest()
        : m_settings{m_tempDir.filePath(u"settings.ini"_s)}
    {
        m_settings.createSetting<Settings::Core::Internal::RemoteReadAheadKb>(64, u"ReadAhead"_s);
        m_settings.createSetting<Settings::Core::Internal::RemoteOpenTimeoutMs>(1000, u"OpenTimeout"_s);
    }

    static void SetUpTestSuite()
    {
        ensureCoreApplication();
    }

    void TearDown() override
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    QTemporaryDir m_tempDir;
    SettingsManager m_settings;
};

TEST_F(RemoteIoServiceTest, DownloadsUsingSharedNetworkManager)
{
    auto network = makeFakeNetworkAccessManager(true);
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
    auto network = makeFakeNetworkAccessManager(true);
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
    auto network = makeFakeNetworkAccessManager(true);
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

TEST_F(RemoteIoServiceTest, PreparesStreamsUsingSharedNetworkManager)
{
    auto network = makeFakeNetworkAccessManager(true);
    RemoteIoService service{network, &m_settings};

    for(const auto& url : {u"https://example.com/track.mp3"_s, u"https://example.com/live.m3u8"_s}) {
        auto creation = std::async(std::launch::async, [&service, url]() { return service.prepareStream(QUrl{url}); });
        auto pending  = creation.get();
        EXPECT_EQ(pending->thread(), network->thread());
        bool finished{false};
        QObject::connect(pending.get(), &PendingRemoteStream::finished, qApp, [&]() { finished = true; });
        QCoreApplication::processEvents();
        ASSERT_FALSE(network->lastReply.isNull());
        EXPECT_EQ(networkUserAgent(), network->lastRequest.rawHeader("User-Agent"));
        network->lastReply->setResponseHeader("Content-Length", "4");
        network->lastReply->emitMetadataChanged();
        ASSERT_TRUE(waitFor([&] { return finished; }));
        EXPECT_TRUE(pending->isFinished());
        EXPECT_TRUE(pending->isReady());
        auto source = pending->takeSource();
        ASSERT_NE(source.device, nullptr);
        EXPECT_TRUE(source.device->isOpen());
        EXPECT_TRUE(source.device->isSequential());
    }
}

TEST_F(RemoteIoServiceTest, EmitsFinishedOnceForPreparedStream)
{
    auto network = makeFakeNetworkAccessManager(true);
    RemoteIoService service{network, &m_settings};
    auto pending = service.prepareStream(QUrl{u"https://example.com/track.mp3"_s});
    int completions{0};
    QObject::connect(pending.get(), &PendingRemoteStream::finished, qApp, [&]() {
        EXPECT_EQ(QThread::currentThread(), network->thread());
        EXPECT_TRUE(pending->isReady());
        ++completions;
    });
    QCoreApplication::processEvents();
    ASSERT_FALSE(network->lastReply.isNull());
    network->lastReply->setResponseHeader("Content-Length", "4");
    network->lastReply->emitMetadataChanged();
    ASSERT_TRUE(waitFor([&] { return completions == 1; }));

    auto source = pending->takeSource();
    ASSERT_NE(source.device, nullptr);
    bool lateNotification{false};
    QObject::connect(pending.get(), &PendingRemoteStream::finished, qApp, [&]() { lateNotification = true; });
    pending->cancel();
    QCoreApplication::processEvents();
    EXPECT_TRUE(pending->isFinished());
    EXPECT_TRUE(pending->isReady());
    EXPECT_EQ(completions, 1);
    EXPECT_FALSE(lateNotification);
}

TEST_F(RemoteIoServiceTest, DisconnectsDestroyedStreamReceiver)
{
    auto network = makeFakeNetworkAccessManager(true);
    RemoteIoService service{network, &m_settings};
    auto pending = service.prepareStream(QUrl{u"https://example.com/track.mp3"_s});
    auto context = std::make_unique<QObject>();
    bool notified{false};
    QObject::connect(pending.get(), &PendingRemoteStream::finished, context.get(), [&]() { notified = true; });
    int completions{0};
    QObject::connect(pending.get(), &PendingRemoteStream::finished, qApp, [&]() { ++completions; });
    context.reset();
    QCoreApplication::processEvents();
    ASSERT_FALSE(network->lastReply.isNull());
    network->lastReply->setResponseHeader("Content-Length", "4");
    network->lastReply->emitMetadataChanged();

    ASSERT_TRUE(waitFor([&] { return completions == 1; }));
    EXPECT_TRUE(pending->isReady());
    EXPECT_FALSE(notified);
}

TEST_F(RemoteIoServiceTest, CancelsStreamPreparationWhenHandleDropped)
{
    using namespace std::chrono_literals;

    auto network = makeFakeNetworkAccessManager(true);
    RemoteIoService service{network, &m_settings};
    auto pending = service.prepareStream(QUrl{u"https://example.com/track.mp3"_s});
    int completions{0};
    QObject::connect(pending.get(), &PendingRemoteStream::finished, qApp, [&]() { ++completions; });
    QCoreApplication::processEvents();
    const auto reply = network->lastReply;
    ASSERT_FALSE(reply.isNull());

    std::promise<QThread*> destroyed;
    auto destruction = destroyed.get_future();
    QObject::connect(
        pending.get(), &QObject::destroyed, qApp, [&destroyed]() { destroyed.set_value(QThread::currentThread()); },
        Qt::DirectConnection);
    auto drop = std::async(std::launch::async, [operation = std::move(pending)]() mutable { operation.reset(); });
    drop.get();
    ASSERT_TRUE(waitFor([&]() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        return destruction.wait_for(0ms) == std::future_status::ready;
    }));
    EXPECT_EQ(destruction.get(), network->thread());
    ASSERT_TRUE(waitFor([&] { return reply.isNull() || reply->wasAborted(); }));
    EXPECT_EQ(completions, 0);
}

TEST_F(RemoteIoServiceTest, CancelsStreamPreparationBeforeRequestStarts)
{
    auto network = makeFakeNetworkAccessManager(true);
    RemoteIoService service{network, &m_settings};
    auto pending = service.prepareStream(QUrl{u"https://example.com/track.mp3"_s});
    int completions{0};
    QObject::connect(pending.get(), &PendingRemoteStream::finished, qApp, [&]() { ++completions; });

    pending->cancel();
    ASSERT_TRUE(waitFor([&] { return completions == 1; }));
    EXPECT_EQ(network->requestCount, 0);
    EXPECT_TRUE(pending->isFinished());
    EXPECT_FALSE(pending->isReady());
    EXPECT_EQ(pending->errorString(), u"Cancelled"_s);
    EXPECT_FALSE(pending->takeSource().device);
}

TEST_F(RemoteIoServiceTest, TimesOutStreamPreparation)
{
    auto network = makeFakeNetworkAccessManager(true);
    RemoteIoService service{network, &m_settings};
    auto pending = service.prepareStream(QUrl{u"https://example.com/slow.mp3"_s});
    int completions{0};
    QObject::connect(pending.get(), &PendingRemoteStream::finished, qApp, [&]() { ++completions; });
    QCoreApplication::processEvents();
    const auto reply = network->lastReply;
    ASSERT_FALSE(reply.isNull());

    ASSERT_TRUE(waitFor([&] { return completions == 1 && (reply.isNull() || reply->wasAborted()); }));
    EXPECT_TRUE(pending->isFinished());
    EXPECT_FALSE(pending->isReady());
    EXPECT_EQ(pending->errorString(), u"Timed out preparing remote stream."_s);
    EXPECT_FALSE(pending->takeSource().device);
}

TEST_F(RemoteIoServiceTest, TimesOutWaitingForStreamPreparation)
{
    using namespace std::chrono_literals;

    auto network = makeFakeNetworkAccessManager(true);
    RemoteIoService service{network, &m_settings};
    auto pending = service.prepareStream(QUrl{u"https://example.com/slow.mp3"_s});
    int completions{0};
    QObject::connect(pending.get(), &PendingRemoteStream::finished, qApp, [&]() { ++completions; });
    QCoreApplication::processEvents();
    const auto reply = network->lastReply;
    ASSERT_FALSE(reply.isNull());

    auto operation = std::async(std::launch::async, [&]() { return pending->waitForReady(1ms); });
    ASSERT_TRUE(waitFor([&] { return operation.wait_for(0ms) == std::future_status::ready; }));
    EXPECT_FALSE(operation.get());
    ASSERT_TRUE(waitFor([&] { return completions == 1 && (reply.isNull() || reply->wasAborted()); }));
    EXPECT_TRUE(pending->isFinished());
    EXPECT_FALSE(pending->isReady());
    EXPECT_EQ(pending->errorString(), u"Timed out preparing remote stream."_s);
    EXPECT_FALSE(pending->takeSource().device);
}

TEST_F(RemoteIoServiceTest, PreparesHttpCapabilitiesBeforeExposingDevice)
{
    using namespace std::chrono_literals;

    const QByteArray audio{"abcdefghijklmnopqrstuvwxyz"};
    const auto network = makeFakeNetworkAccessManager(true);
    const RemoteIoService service{network, &m_settings};
    std::stop_source abortSource;
    const auto finishOperation = [&abortSource](auto& operation) {
        const bool ready = waitFor([&]() { return operation.wait_for(0ms) == std::future_status::ready; });
        if(!ready) {
            abortSource.request_stop();
        }
        EXPECT_TRUE(ready);
        return operation.get();
    };
    int completions{0};
    auto pending = service.prepareStream(QUrl{u"https://example.com/track.mp3"_s});
    QObject::connect(pending.get(), &PendingRemoteStream::finished, qApp, [&]() {
        EXPECT_EQ(QThread::currentThread(), qApp->thread());
        EXPECT_TRUE(pending->isReady());
        ++completions;
    });
    EXPECT_FALSE(pending->isFinished());
    EXPECT_FALSE(pending->takeSource().device);
    EXPECT_FALSE(pending->waitForReady(1s));
    EXPECT_FALSE(pending->isFinished());
    QCoreApplication::processEvents();
    ASSERT_FALSE(network->lastReply.isNull());
    EXPECT_FALSE(pending->takeSource().device);

    auto preparation
        = std::async(std::launch::async, [&]() { return pending->waitForReady(1s, abortSource.get_token()); });
    network->lastReply->setResponseHeader("Content-Length", QByteArray::number(audio.size()));
    network->lastReply->setResponseHeader("Accept-Ranges", "bytes");
    network->lastReply->emitMetadataChanged();
    ASSERT_TRUE(finishOperation(preparation));
    QCoreApplication::processEvents();
    EXPECT_EQ(completions, 1);
    EXPECT_EQ(network->lastReply->bytesAvailable(), 0);

    network->onRequest = [&audio](FakeNetworkReply* reply) {
        const qint64 offset = reply->request().rawHeader("Range").sliced(6).chopped(1).toLongLong();
        reply->setResponseStatus(206);
        reply->setResponseHeader("Content-Range", "bytes " + QByteArray::number(offset) + '-'
                                                      + QByteArray::number(audio.size() - 1) + '/'
                                                      + QByteArray::number(audio.size()));
        reply->setResponseHeader("Content-Length", QByteArray::number(audio.size() - offset));
        reply->setResponseHeader("Accept-Ranges", "bytes");
        reply->finish(audio.sliced(offset));
    };

    std::promise<void> readAttempted;
    auto earlyRead = readAttempted.get_future();
    auto operation = std::async(std::launch::async, [&]() {
        auto source = pending->takeSource();
        if(!source.device) {
            readAttempted.set_value();
            FAIL() << "Prepared source has no device";
        }
        EXPECT_TRUE(source.device->isOpen());
        EXPECT_EQ(source.device->thread(), QThread::currentThread());
        EXPECT_FALSE(source.device->isSequential());
        EXPECT_EQ(source.device->size(), audio.size());
        source.remoteDevice->setReadCancellationToken(abortSource.get_token());
        source.remoteDevice->setNonBlockingReadsEnabled(true);
        char data{0};
        EXPECT_EQ(source.device->read(&data, 1), -1);
        EXPECT_TRUE(source.remoteDevice->readWouldBlock());
        EXPECT_EQ(source.device->pos(), 0);
        readAttempted.set_value();
        source.remoteDevice->setNonBlockingReadsEnabled(false);
        EXPECT_EQ(source.device->read(4), "abcd");
        EXPECT_EQ(source.device->pos(), 4);
        ASSERT_TRUE(source.device->seek(8));
        EXPECT_EQ(source.device->pos(), 8);
        EXPECT_EQ(source.device->read(3), "ijk");
        EXPECT_EQ(source.device->pos(), 11);
        EXPECT_FALSE(pending->takeSource().device);
    });

    finishOperation(earlyRead);
    network->lastReply->finish(audio);
    finishOperation(operation);
    EXPECT_EQ(network->lastRequest.rawHeader("Range"), "bytes=8-");
    EXPECT_EQ(network->requestCount, 2);
}
} // namespace Fooyin::Testing
