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

#include <core/engine/input/id3utils.h>
#include <core/network/hlsstreamdevice.h>
#include <core/network/networkstreamdevice.h>
#include <core/network/networkutils.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QNetworkReply>
#include <QPointer>
#include <QStandardPaths>
#include <QtEndian>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <stop_token>
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

    void setResponseHeader(const QByteArray& name, const QByteArray& value)
    {
        setRawHeader(name, value);
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

QByteArray icyStreamData(QByteArrayView audio, QByteArray metadata)
{
    static constexpr int IcyMetadataBlockSize = 16;

    const qsizetype blockCount = (metadata.size() + IcyMetadataBlockSize - 1) / IcyMetadataBlockSize;
    metadata.append(QByteArray((blockCount * IcyMetadataBlockSize) - metadata.size(), '\0'));

    QByteArray data{audio.data(), audio.size()};
    data.append(static_cast<char>(blockCount));
    data.append(metadata);
    return data;
}

void appendBigEndian32(QByteArray& data, quint32 value)
{
    char encoded[4];
    qToBigEndian(value, encoded);
    data.append(encoded, 4);
}

void appendBigEndian64(QByteArray& data, quint64 value)
{
    char encoded[8];
    qToBigEndian(value, encoded);
    data.append(encoded, 8);
}

void appendSynchsafe32(QByteArray& data, quint32 value)
{
    data.append(static_cast<char>((value >> 21U) & 0x7FU));
    data.append(static_cast<char>((value >> 14U) & 0x7FU));
    data.append(static_cast<char>((value >> 7U) & 0x7FU));
    data.append(static_cast<char>(value & 0x7FU));
}

QByteArray id3TextFrame(QByteArrayView id, QByteArrayView value)
{
    QByteArray payload;
    payload.append(char{3}); // UTF-8
    payload.append(value);

    QByteArray frame{id.data(), id.size()};
    appendSynchsafe32(frame, static_cast<quint32>(payload.size()));
    frame.append(QByteArray{2, '\0'});
    frame.append(payload);
    return frame;
}

QByteArray timedId3Emsg(QByteArrayView artist, QByteArrayView title, QByteArrayView station)
{
    QByteArray frames;
    frames.append(id3TextFrame("TPE1", artist));
    frames.append(id3TextFrame("TIT2", title));
    frames.append(id3TextFrame("TRSN", station));

    QByteArray id3{"ID3\x04\x00\x00", 6};
    appendSynchsafe32(id3, static_cast<quint32>(frames.size()));
    id3.append(frames);

    QByteArray payload{4, '\0'};
    payload[0] = char{1}; // emsg version 1
    appendBigEndian32(payload, 48'000);
    appendBigEndian64(payload, 0);
    appendBigEndian32(payload, 0);
    appendBigEndian32(payload, 1);
    payload.append("https://developer.apple.com/streaming/emsg-id3");
    payload.append('\0');
    payload.append('\0'); // Empty value
    payload.append(id3);

    QByteArray box;
    appendBigEndian32(box, static_cast<quint32>(8 + payload.size()));
    box.append("emsg");
    box.append(payload);
    return box;
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

TEST_F(NetworkStreamDeviceTest, ParsesMpegTsTimedId3Payload)
{
    const QByteArray payload = QByteArray::fromHex(
        "49443303000000000035544954320000000fc00003436c6f73652054686520446f6f725450453100000012c0000354656464"
        "792050656e6465726772617373");

    const auto metadata = Id3Utils::parseTimedMetadata(payload);

    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->title, u"Close The Door"_s);
    EXPECT_EQ(metadata->artist, u"Teddy Pendergrass"_s);
    EXPECT_TRUE(metadata->station.isEmpty());
}

TEST_F(NetworkStreamDeviceTest, ParsesMpegTsTimedId3PayloadStrippedByFfmpeg5)
{
    const QByteArray payload = QByteArray::fromHex(
        "49443303000000000035544954320000000fc00003436c6f73652054686520446f6f725450453100000012c0000354656464"
        "792050656e6465726772617373");

    const auto metadata = Id3Utils::parseTimedMetadata(QByteArrayView{payload}.sliced(5));

    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->title, u"Close The Door"_s);
    EXPECT_EQ(metadata->artist, u"Teddy Pendergrass"_s);
}

TEST_F(NetworkStreamDeviceTest, PreservesApostrophesInIcyMetadataFields)
{
    auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    ASSERT_FALSE(network->lastReply.isNull());

    network->lastReply->setResponseHeader("icy-metaint", "4");
    network->lastReply->appendData(icyStreamData("data", "StreamTitle='Kenny G - Don't Make Me Wait for Love';"
                                                         "StreamUrl='https://radio.example.com/art';"));
    network->lastReply->emitReadyRead();

    const auto metadata = device.metadata();
    EXPECT_EQ(u"Kenny G - Don't Make Me Wait for Love"_s, metadata.streamTitle);
    EXPECT_EQ(u"https://radio.example.com/art"_s, metadata.streamUrl);
}

TEST_F(NetworkStreamDeviceTest, UnescapesApostrophesInIcyMetadataFields)
{
    auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    ASSERT_FALSE(network->lastReply.isNull());

    network->lastReply->setResponseHeader("icy-metaint", "4");
    network->lastReply->appendData(icyStreamData("data", "StreamTitle='Guns N\\' Roses - Patience';"));
    network->lastReply->emitReadyRead();

    EXPECT_EQ(u"Guns N' Roses - Patience"_s, device.metadata().streamTitle);
}

TEST_F(NetworkStreamDeviceTest, MatchesIcyMetadataKeysOnlyAtFieldBoundaries)
{
    auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    ASSERT_FALSE(network->lastReply.isNull());

    network->lastReply->setResponseHeader("icy-metaint", "4");
    network->lastReply->appendData(icyStreamData("data", "NotStreamTitle='wrong'; StreamTitle='Artist - Correct';"));
    network->lastReply->emitReadyRead();

    EXPECT_EQ(u"Artist - Correct"_s, device.metadata().streamTitle);
}

TEST_F(NetworkStreamDeviceTest, RejectsUnterminatedIcyMetadataFields)
{
    auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    ASSERT_FALSE(network->lastReply.isNull());

    network->lastReply->setResponseHeader("icy-metaint", "4");
    network->lastReply->appendData(icyStreamData("data", "StreamTitle='unfinished"));
    network->lastReply->emitReadyRead();

    EXPECT_TRUE(device.metadata().streamTitle.isEmpty());
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

TEST_F(NetworkStreamDeviceTest, CancellationTokenWakesBlockingRead)
{
    using namespace std::chrono_literals;

    auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};
    std::stop_source abortSource;

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    device.setReadCancellationToken(abortSource.get_token());
    device.setNonBlockingReadsEnabled(false);

    auto read = std::async(std::launch::async, [&device]() {
        char data{0};
        return device.read(&data, 1);
    });

    EXPECT_EQ(read.wait_for(50ms), std::future_status::timeout);

    abortSource.request_stop();

    ASSERT_EQ(read.wait_for(500ms), std::future_status::ready);
    EXPECT_LT(read.get(), 0);
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

TEST_F(NetworkStreamDeviceTest, HlsReadsTimedId3MetadataFromEmsg)
{
    auto network = makeFakeNetworkAccessManager();
    HlsStreamDevice device{network, QUrl{u"https://radio.example.com/live/playlist.m3u8"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));

    QPointer<FakeReply> playlistReply = network->lastReply;
    ASSERT_FALSE(playlistReply.isNull());
    playlistReply->appendData("#EXTM3U\n"
                              "#EXTINF:3,\n"
                              "segment-1.m4s\n"
                              "#EXTINF:3,\n"
                              "segment-2.m4s\n"
                              "#EXT-X-ENDLIST\n");
    playlistReply->emitFinished();
    QCoreApplication::processEvents();

    QPointer<FakeReply> segmentReply = network->lastReply;
    ASSERT_FALSE(segmentReply.isNull());
    const QByteArray firstMetadata = timedId3Emsg("Paul Carrack", "Don't Shed a Tear", "LG73");
    segmentReply->appendData(firstMetadata.first(17));
    segmentReply->emitReadyRead();
    QCoreApplication::processEvents();

    ASSERT_TRUE(device.remoteStreamMetadata().has_value());
    EXPECT_EQ(device.remoteStreamMetadata()->revision, 0);

    segmentReply->appendData(firstMetadata.sliced(17));
    segmentReply->emitReadyRead();
    QCoreApplication::processEvents();

    const auto metadata = device.remoteStreamMetadata();
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->streamTitle, u"Paul Carrack - Don't Shed a Tear"_s);
    EXPECT_EQ(metadata->streamName, u"LG73"_s);
    EXPECT_EQ(metadata->revision, 1);

    auto read = std::async(std::launch::async, [&device, size = firstMetadata.size()]() { return device.read(size); });
    ASSERT_EQ(read.wait_for(std::chrono::milliseconds{500}), std::future_status::ready);
    EXPECT_EQ(read.get(), firstMetadata);

    segmentReply->emitFinished();
    QCoreApplication::processEvents();

    QPointer<FakeReply> nextSegmentReply = network->lastReply;
    ASSERT_FALSE(nextSegmentReply.isNull());
    ASSERT_NE(nextSegmentReply, segmentReply);
    nextSegmentReply->appendData(timedId3Emsg("The Police", "Message In A Bottle", "LG73"));
    nextSegmentReply->emitReadyRead();
    QCoreApplication::processEvents();

    const auto updatedMetadata = device.remoteStreamMetadata();
    ASSERT_TRUE(updatedMetadata.has_value());
    EXPECT_EQ(updatedMetadata->streamTitle, u"The Police - Message In A Bottle"_s);
    EXPECT_EQ(updatedMetadata->streamName, u"LG73"_s);
    EXPECT_EQ(updatedMetadata->revision, 2);
}
} // namespace Fooyin::Testing
