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

#include "testutils.h"

#include <core/engine/input/ffmpeg/ffmpeginput.h>
#include <core/engine/input/metadata/id3.h>
#include <core/network/hlsstreamdevice.h>
#include <core/network/networkstreamdevice.h>
#include <core/network/networkutils.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QPointer>
#include <QStandardPaths>
#include <QTimeZone>
#include <QtEndian>

#include <array>
#include <chrono>
#include <future>
#include <stop_token>
#include <utility>

using namespace Qt::StringLiterals;

namespace {
template <typename T>
T finishNetworkOperation(std::future<T>& operation, std::stop_source& abortSource)
{
    using namespace std::chrono_literals;

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while(operation.wait_for(0ms) != std::future_status::ready && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    if(operation.wait_for(0ms) != std::future_status::ready) {
        ADD_FAILURE() << "Network operation timed out";
        abortSource.request_stop();
    }
    return operation.get();
}

void serveHttpFile(Fooyin::Testing::FakeNetworkReply* reply, const QByteArray& data)
{
    const QByteArray range = reply->request().rawHeader("Range");
    qint64 offset{0};
    if(!range.isEmpty()) {
        offset = range.sliced(6).chopped(1).toLongLong();
        reply->setResponseStatus(206);
        reply->setResponseHeader("Content-Range", "bytes " + QByteArray::number(offset) + '-'
                                                      + QByteArray::number(data.size() - 1) + '/'
                                                      + QByteArray::number(data.size()));
    }
    reply->setResponseHeader("Content-Length", QByteArray::number(data.size() - offset));
    reply->setResponseHeader("Accept-Ranges", "bytes");
    reply->finish(data.sliced(offset));
}

QByteArray makeHttpWave()
{
    QByteArray wave;
    const auto append16 = [&wave](quint16 value) {
        char data[2];
        qToLittleEndian(value, data);
        wave.append(data, 2);
    };
    const auto append32 = [&wave](quint32 value) {
        char data[4];
        qToLittleEndian(value, data);
        wave.append(data, 4);
    };

    wave.append("RIFF");
    append32(36 + 88'200);
    wave.append("WAVEfmt ");
    append32(16);
    append16(1);
    append16(1);
    append32(44'100);
    append32(88'200);
    append16(2);
    append16(16);
    wave.append("data");
    append32(88'200);
    for(int sample{0}; sample < 44'100; ++sample) {
        append16(sample < 22'050 ? 1234 : 4321);
    }
    return wave;
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
    void TearDown() override
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

TEST_F(NetworkStreamDeviceTest, SendsStreamRequestHeaders)
{
    const auto network = makeFakeNetworkAccessManager();
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

    const auto metadata = Metadata::parseTimedId3Metadata(payload);

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

    const auto metadata = Metadata::parseTimedId3Metadata(QByteArrayView{payload}.sliced(5));

    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->title, u"Close The Door"_s);
    EXPECT_EQ(metadata->artist, u"Teddy Pendergrass"_s);
}

TEST_F(NetworkStreamDeviceTest, PreservesApostrophesInIcyMetadataFields)
{
    const auto network = makeFakeNetworkAccessManager();
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
    const auto network = makeFakeNetworkAccessManager();
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
    const auto network = makeFakeNetworkAccessManager();
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
    const auto network = makeFakeNetworkAccessManager();
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
    const auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));

    char data{0};
    EXPECT_LT(device.read(&data, 1), 0);
    EXPECT_FALSE(device.errorString().isEmpty());
}

TEST_F(NetworkStreamDeviceTest, CancellationTokenWakesBlockingRead)
{
    using namespace std::chrono_literals;

    const auto network = makeFakeNetworkAccessManager();
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
    const auto network = makeFakeNetworkAccessManager();
    QPointer<FakeNetworkReply> reply;

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

TEST_F(NetworkStreamDeviceTest, DetectsFiniteHttpFileCapabilities)
{
    struct Response
    {
        QByteArray length;
        QByteArray ranges;
        QByteArray icyInterval;
        QByteArray encoding;
        qint64 expectedSize;
        bool sequential;
    };
    static const std::array responses{
        Response{"100", "bytes", {}, {}, 100, false},   Response{"100", {}, {}, {}, 100, true},
        Response{{}, "bytes", {}, {}, -1, true},        Response{"100", "bytes", "4", {}, -1, true},
        Response{"100", "bytes", {}, "gzip", -1, true}, Response{"invalid", "bytes", {}, {}, -1, true},
    };

    for(const auto& response : responses) {
        const auto network = makeFakeNetworkAccessManager();
        NetworkStreamDevice device{network, QUrl{u"https://example.com/track.mp3"_s}, 64 * 1024};
        ASSERT_TRUE(device.open(QIODevice::ReadOnly));

        auto* reply = network->lastReply.data();
        ASSERT_NE(reply, nullptr);
        reply->setResponseHeader("Content-Length", response.length);
        reply->setResponseHeader("Accept-Ranges", response.ranges);
        reply->setResponseHeader("icy-metaint", response.icyInterval);
        reply->setResponseHeader("Content-Encoding", response.encoding);
        reply->emitMetadataChanged();

        EXPECT_EQ(device.size(), response.expectedSize);
        EXPECT_EQ(device.isSequential(), response.sequential);
        EXPECT_EQ(network->lastRequest.rawHeader("Accept-Encoding"), "identity");
    }
}

TEST_F(NetworkStreamDeviceTest, SeeksUsingValidatedHttpRanges)
{
    const QByteArray audio{"abcdefghijklmnopqrstuvwxyz"};
    const auto network = makeFakeNetworkAccessManager(true);
    network->onRequest = [&audio](FakeNetworkReply* reply) {
        serveHttpFile(reply, audio);
    };
    NetworkStreamDevice device{network, QUrl{u"https://example.com/track.mp3"_s}, 64 * 1024};
    std::stop_source abortSource;
    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    device.setReadCancellationToken(abortSource.get_token());

    auto operation = std::async(std::launch::async, [&device, &audio]() {
        ASSERT_TRUE(device.waitForReadyRead(1000));
        EXPECT_FALSE(device.isSequential());
        EXPECT_EQ(device.read(4), "abcd");
        EXPECT_EQ(device.pos(), 4);
        ASSERT_TRUE(device.seek(8));
        EXPECT_EQ(device.pos(), 8);
        EXPECT_EQ(device.size(), audio.size());
        EXPECT_EQ(device.read(3), "ijk");
        ASSERT_TRUE(device.seek(0));
        EXPECT_EQ(device.read(3), "abc");
        ASSERT_TRUE(device.seek(audio.size()));
        char data{0};
        EXPECT_EQ(device.read(&data, 1), 0);
        ASSERT_TRUE(device.seek(5));
        EXPECT_EQ(device.read(3), "fgh");
        EXPECT_FALSE(device.seek(-1));
        EXPECT_FALSE(device.seek(audio.size() + 1));
    });
    finishNetworkOperation(operation, abortSource);
    EXPECT_EQ(network->lastRequest.rawHeader("Range"), "bytes=5-");
    EXPECT_EQ(network->requestCount, 4);
}

TEST_F(NetworkStreamDeviceTest, ReadsDurationAndSeeksInFiniteHttpAudio)
{
    using namespace std::chrono_literals;

    const QByteArray wave = makeHttpWave();
    const auto network    = makeFakeNetworkAccessManager(true);
    const QDateTime modifiedTime{QDate{2020, 11, 24}, QTime{10, 3, 40}, QTimeZone::utc()};
    network->onRequest = [&wave](FakeNetworkReply* reply) {
        reply->setResponseHeader("Last-Modified", "Tue, 24 Nov 2020 10:03:40 GMT");
        serveHttpFile(reply, wave);
    };

    NetworkStreamDevice device{network, QUrl{u"https://example.com/track.wav"_s}, 64 * 1024};
    device.setOpenTimeout(1s);
    ASSERT_TRUE(device.open(QIODevice::ReadOnly));

    const Track track{u"https://example.com/track.wav"_s};
    const AudioSource source{
        .filepath = track.filepath(), .device = &device, .remoteStreamDevice = &device, .remoteOpenTimeout = 1s};
    std::stop_source abortSource;
    FFmpegDecoder decoder;
    std::stop_callback abortDecoder{abortSource.get_token(), [&decoder]() { decoder.requestAbort(); }};

    auto operation = std::async(std::launch::async, [&]() {
        {
            FFmpegReader reader;
            ASSERT_TRUE(reader.init(source));
            Track metadataTrack{track};
            ASSERT_TRUE(reader.readTrack(source, metadataTrack));
            EXPECT_EQ(metadataTrack.duration(), 1000);
            EXPECT_EQ(metadataTrack.fileSize(), wave.size());
            EXPECT_EQ(metadataTrack.modifiedTime(), modifiedTime.toMSecsSinceEpoch());
        }

        ASSERT_TRUE(device.seek(0));
        ASSERT_TRUE(decoder.init(source, track, AudioDecoder::UpdateTracks).has_value());
        EXPECT_TRUE(decoder.isSeekable());
        EXPECT_TRUE(decoder.trackHasChanged());
        EXPECT_EQ(decoder.changedTrack().duration(), 1000);
        EXPECT_EQ(decoder.changedTrack().fileSize(), wave.size());
        EXPECT_EQ(decoder.changedTrack().modifiedTime(), modifiedTime.toMSecsSinceEpoch());

        decoder.start();
        decoder.seek(500);
        auto result = decoder.readAudio(4096);
        ASSERT_EQ(result.status, AudioDecoder::ReadStatus::DecodedAudio);
        EXPECT_EQ(result.buffer.startTime(), 500);

        qint16 sample{0};
        std::memcpy(&sample, result.buffer.constData().data(), sizeof(sample));
        EXPECT_EQ(sample, 4321);

        decoder.seek(100);
        result = decoder.readAudio(4096);
        ASSERT_EQ(result.status, AudioDecoder::ReadStatus::DecodedAudio);
        EXPECT_EQ(result.buffer.startTime(), 100);

        std::memcpy(&sample, result.buffer.constData().data(), sizeof(sample));
        EXPECT_EQ(sample, 1234);

        for(int attempt{0}; attempt < 100; ++attempt) {
            result = decoder.readAudio(4096);
            if(result.status != AudioDecoder::ReadStatus::DecodedAudio) {
                break;
            }
        }
        EXPECT_EQ(result.status, AudioDecoder::ReadStatus::EndOfStream);
        decoder.stop();
    });

    finishNetworkOperation(operation, abortSource);
    const int requests = network->requestCount;
    QCoreApplication::processEvents();
    EXPECT_EQ(network->requestCount, requests);
}

TEST_F(NetworkStreamDeviceTest, DrainsFinishedReplyBeforeReturningEOS)
{
    using namespace std::chrono_literals;

    static constexpr qsizetype BufferBytes = 64 * 1024UL;

    const auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://example.com/track.mp3"_s}, BufferBytes};
    std::stop_source abortSource;

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    device.setReadCancellationToken(abortSource.get_token());

    const QPointer<FakeNetworkReply> reply = network->lastReply;
    ASSERT_FALSE(reply.isNull());

    QByteArray audio(BufferBytes, 'a');
    audio.append(QByteArray(BufferBytes, 'b'));
    audio.append(QByteArray(BufferBytes + 17, 'c'));
    reply->finish(audio);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    EXPECT_FALSE(reply.isNull());
    EXPECT_EQ(device.bufferedByteCount(), BufferBytes);

    auto read = std::async(std::launch::async, [&device]() {
        QByteArray received;
        char buffer[4096];
        while(true) {
            const qint64 sizeRead = device.read(buffer, sizeof(buffer));
            if(sizeRead <= 0) {
                return std::pair{received, sizeRead};
            }
            received.append(buffer, sizeRead);
        }
    });

    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while(read.wait_for(0ms) != std::future_status::ready && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    EXPECT_EQ(read.wait_for(0ms), std::future_status::ready);
    abortSource.request_stop();

    const auto [received, lastRead] = read.get();
    EXPECT_EQ(received, audio);
    EXPECT_EQ(lastRead, 0);

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(reply.isNull());
    EXPECT_EQ(network->requestCount, 1);
}

TEST_F(NetworkStreamDeviceTest, ReconnectsAfterCleanFinishWhenEnabled)
{
    using namespace std::chrono_literals;

    const auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};
    std::stop_source abortSource;

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    device.setReadCancellationToken(abortSource.get_token());
    device.setReconnectOnFinishedEnabled(true);

    const QPointer<FakeNetworkReply> firstReply = network->lastReply;
    ASSERT_FALSE(firstReply.isNull());

    const QByteArray audio((192 * 1024) + 17, 'a');
    firstReply->finish(audio);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    EXPECT_FALSE(firstReply.isNull());
    EXPECT_EQ(network->requestCount, 1);

    auto read = std::async(std::launch::async, [&device, &audio]() {
        QByteArray received;
        while(received.size() < audio.size()) {
            const QByteArray data = device.read(4096);
            if(data.isEmpty()) {
                break;
            }
            received.append(data);
        }
        return received;
    });

    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while(read.wait_for(0ms) != std::future_status::ready && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    abortSource.request_stop();
    EXPECT_EQ(read.get(), audio);
    QCoreApplication::processEvents();

    const QPointer<FakeNetworkReply> secondReply = network->lastReply;
    ASSERT_FALSE(secondReply.isNull());
    EXPECT_NE(firstReply, secondReply);
    EXPECT_EQ(network->requestCount, 2);
}

TEST_F(NetworkStreamDeviceTest, StopsReconnectLoopAfterRepeatedEmptyCleanFinishes)
{
    const auto network = makeFakeNetworkAccessManager();
    NetworkStreamDevice device{network, QUrl{u"https://radio.example.com/live"_s}, 2048};

    ASSERT_TRUE(device.open(QIODevice::ReadOnly));
    device.setNonBlockingReadsEnabled(true);
    device.setReconnectOnFinishedEnabled(true);

    static constexpr int MaxReconnects = 5;
    ASSERT_EQ(network->requestCount, 1);

    for(int i{0}; i < MaxReconnects; ++i) {
        const QPointer<FakeNetworkReply> reply = network->lastReply;
        ASSERT_FALSE(reply.isNull());

        reply->emitFinished();
        QCoreApplication::processEvents();

        ASSERT_EQ(network->requestCount, i + 2);
        ASSERT_FALSE(network->lastReply.isNull());
        EXPECT_NE(reply, network->lastReply);
    }

    const QPointer<FakeNetworkReply> finalReply = network->lastReply;
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

    const QPointer<FakeNetworkReply> playlistReply = network->lastReply;
    ASSERT_FALSE(playlistReply.isNull());
    playlistReply->appendData("#EXTM3U\n"
                              "#EXTINF:3,\n"
                              "segment-1.m4s\n"
                              "#EXTINF:3,\n"
                              "segment-2.m4s\n"
                              "#EXT-X-ENDLIST\n");
    playlistReply->emitFinished();
    QCoreApplication::processEvents();

    const QPointer<FakeNetworkReply> segmentReply = network->lastReply;
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

    const QPointer<FakeNetworkReply> nextSegmentReply = network->lastReply;
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

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    const QCoreApplication application{argc, argv};
    QCoreApplication::setApplicationName(u"fooyin-networkstreamdevice-test"_s);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
