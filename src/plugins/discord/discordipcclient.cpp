/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <luket@pm.me>
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

#include "discordipcclient.h"

#include "discordmessage.h"

#include <utils/fypaths.h>

#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTimer>
#include <QUuid>

#include <QCoro/QCoroLocalSocket>

Q_LOGGING_CATEGORY(DISCORD, "fy.discord")

using namespace Qt::StringLiterals;

namespace {
constexpr auto MaxPipeCount   = 10;
constexpr auto ConnectTimeout = 500;
constexpr auto ReplyTimeout   = 5000;

enum Opcodes : int
{
    Handshake = 0,
    Frame     = 1,
    Close     = 2,
    Ping      = 3,
    Pong      = 4
};

struct MessageHeader
{
    uint32_t opcode{0};
    uint32_t length{0};
};

QString socketErrorToString(QLocalSocket::LocalSocketError error)
{
    using namespace Qt::Literals::StringLiterals;

    switch(error) {
        case QLocalSocket::ConnectionRefusedError:
            return u"Connection refused"_s;
        case QLocalSocket::PeerClosedError:
            return u"Peer closed the connection"_s;
        case QLocalSocket::ServerNotFoundError:
            return u"Server not found"_s;
        case QLocalSocket::SocketAccessError:
            return u"Access error"_s;
        case QLocalSocket::SocketResourceError:
            return u"Resource error"_s;
        case QLocalSocket::SocketTimeoutError:
            return u"Socket timeout"_s;
        case QLocalSocket::DatagramTooLargeError:
            return u"Datagram too large"_s;
        case QLocalSocket::ConnectionError:
            return u"Network connection error"_s;
        case QLocalSocket::UnsupportedSocketOperationError:
            return u"Unsupported operation"_s;
        case QLocalSocket::UnknownSocketError:
            return u"Unknown error"_s;
        case QLocalSocket::OperationError:
            return u"Operation error"_s;
        default:
            return u"Unrecognized error"_s;
    }
}

void addUniquePath(QStringList& paths, const QString& path)
{
    if(path.isEmpty()) {
        return;
    }

    const QString cleanPath = QDir::cleanPath(path);
    if(!paths.contains(cleanPath)) {
        paths.emplace_back(cleanPath);
    }
}

void addExistingSubDir(QStringList& paths, const QString& basePath, const QString& subDir)
{
    if(basePath.isEmpty()) {
        return;
    }

    const QString path = QDir{basePath}.filePath(subDir);
    if(QFileInfo::exists(path)) {
        addUniquePath(paths, path);
    }
}

QStringList unixPipeDirs()
{
    QStringList dirs;
    const QString runDir = qEnvironmentVariable("XDG_RUNTIME_DIR");

    for(const char* name : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) {
        addUniquePath(dirs, qEnvironmentVariable(name));
    }
    addUniquePath(dirs, u"/tmp"_s);

    const QStringList baseDirs{dirs};
    for(const QString& dir : baseDirs) {
        addExistingSubDir(dirs, dir, u"app/com.discordapp.Discord"_s);
        addExistingSubDir(dirs, dir, u"snap.discord"_s);
    }

    if(!runDir.isEmpty()) {
        const QDir flatpakDir{QDir{runDir}.filePath(u".flatpak"_s)};
        const QFileInfoList appDirs
            = flatpakDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);

        for(const QFileInfo& appDir : appDirs) {
            addExistingSubDir(dirs, appDir.absoluteFilePath(), u"xdg-run"_s);
        }
    }

    return dirs;
}

QStringList pipeLocations()
{
    QStringList locations;

#ifdef Q_OS_WIN
    for(int index{0}; index < MaxPipeCount; ++index) {
        locations.emplace_back(uR"(\\.\pipe\discord-ipc-%1)"_s.arg(index));
    }
#elifdef Q_OS_MAC
    for(int index{0}; index < MaxPipeCount; ++index) {
        locations.emplace_back(QDir::homePath() + u"/Library/Application Support/discord-ipc-%1"_s.arg(index));
    }
#else
    const auto pipeDirs = unixPipeDirs();
    for(const QString& dir : pipeDirs) {
        for(int index{0}; index < MaxPipeCount; ++index) {
            locations.emplace_back(QDir{dir}.filePath(u"discord-ipc-%1"_s.arg(index)));
        }
    }
#endif

    return locations;
}

QJsonObject buildActivityPayload(const Fooyin::Discord::PresenceData& data)
{
    QJsonObject timestamps;
    if(data.start > 0) {
        timestamps["start"_L1] = QJsonValue::fromVariant(data.start);
    }
    if(data.end > 0) {
        timestamps["end"_L1] = QJsonValue::fromVariant(data.end);
    }

    QJsonObject assets;

    const auto addAsset = [&assets](const QString& asset, const auto& key) {
        if(!asset.isEmpty()) {
            assets[key] = QJsonValue{asset};
        }
    };

    addAsset(data.largeImage, "large_image"_L1);
    addAsset(data.largeText, "large_text"_L1);
    addAsset(data.smallImage, "small_image"_L1);
    addAsset(data.smallText, "small_text"_L1);

    const QString nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return {{u"cmd"_s, u"SET_ACTIVITY"_s},
            {u"args"_s, QJsonObject{{u"pid"_s, QCoreApplication::applicationPid()},
                                    {u"activity"_s,
                                     QJsonObject{
                                         {u"type"_s, 2},
                                         {u"instance"_s, false},
                                         {u"state"_s, data.state},
                                         {u"details"_s, data.details},
                                         {u"timestamps"_s, timestamps},
                                         {u"assets"_s, assets},
                                     }}}},
            {u"nonce"_s, nonce}};
}
} // namespace

namespace Fooyin::Discord {
DiscordIPCClient::DiscordIPCClient(QObject* parent)
    : QObject{parent}
    , m_stream{&m_socket}
    , m_connectGeneration{0}
    , m_connectInProgress{false}
    , m_handshakeCompleted{false}
    , m_loggedServerNotFound{false}
{
    m_stream.setByteOrder(QDataStream::LittleEndian);
    m_stream.setVersion(QDataStream::Qt_6_0);

    QObject::connect(&m_socket, &QLocalSocket::errorOccurred, this,
                     [this](QLocalSocket::LocalSocketError error) { logSocketError(error); });

    QObject::connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        qCDebug(DISCORD) << "Disconnected from Discord IPC";
        disconnectFromDiscord();
    });
}

bool DiscordIPCClient::isConnected() const
{
    return m_socket.state() == QLocalSocket::ConnectedState;
}

QString DiscordIPCClient::errorMessage() const
{
    return m_error;
}

void DiscordIPCClient::logSocketError(QLocalSocket::LocalSocketError error)
{
    if(error == QLocalSocket::ServerNotFoundError) {
        if(m_connectInProgress && std::exchange(m_loggedServerNotFound, true)) {
            return;
        }

        qCDebug(DISCORD) << "Socket:" << socketErrorToString(error);
        return;
    }

    qCWarning(DISCORD) << "Socket:" << socketErrorToString(error);
}

QCoro::Task<bool> DiscordIPCClient::connectToDiscord()
{
    if(isConnected()) {
        co_return true;
    }

    if(m_connectInProgress) {
        co_return false;
    }

    if(m_clientId.isEmpty()) {
        setError(u"No Client ID"_s);
        co_return false;
    }

    m_connectInProgress    = true;
    m_loggedServerNotFound = false;

    const quint64 connectGeneration{m_connectGeneration};
    const auto pipeLocs = pipeLocations();

    for(const QString& pipeName : pipeLocs) {
        if(connectGeneration != m_connectGeneration) {
            m_connectInProgress = false;
            co_return false;
        }

        co_await qCoro(m_socket).connectToServer(pipeName, QIODevice::ReadWrite,
                                                 std::chrono::milliseconds{ConnectTimeout});
        if(isConnected()) {
            qCDebug(DISCORD) << "Connected to" << pipeName;

            if(connectGeneration != m_connectGeneration) {
                co_await disconnectFromDiscord();
                m_connectInProgress = false;
                co_return false;
            }

            if(co_await startHandshake()) {
                m_connectInProgress = false;
                co_return true;
            }

            co_await disconnectFromDiscord();
        }
        else if(m_socket.state() != QLocalSocket::UnconnectedState) {
            co_await disconnectFromDiscord();
        }
    }

    setError(u"No IPC pipe available"_s);
    co_await disconnectFromDiscord();
    m_connectInProgress = false;
    co_return false;
}

QCoro::Task<> DiscordIPCClient::disconnectFromDiscord()
{
    if(isConnected()) {
        m_socket.close();
        co_await qCoro(m_socket).waitForDisconnected(ConnectTimeout);
    }

    m_handshakeCompleted = false;
}

QCoro::Task<bool> DiscordIPCClient::updateActivity(const PresenceData data)
{
    if(!isConnected()) {
        co_await connectToDiscord();
    }
    if(!isConnected()) {
        co_return false;
    }

    const QJsonObject payload = buildActivityPayload(data);
    sendMessage(payload, Frame);

    if(co_await waitForReadyRead()) {
        if(auto msg = readMessage()) {
            co_return processMessage(*msg);
        }
    }

    co_return false;
}

QCoro::Task<bool> DiscordIPCClient::clearActivity()
{
    if(!isConnected()) {
        co_return false;
    }

    const QString nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject activityMessage{
        {u"cmd"_s, u"SET_ACTIVITY"_s},
        {u"args"_s, QJsonObject{{u"pid"_s, QCoreApplication::applicationPid()}, {u"activity"_s, QJsonValue::Null}}},
        {u"nonce"_s, nonce}};

    sendMessage(activityMessage, Frame);

    if(co_await waitForReadyRead()) {
        if(auto msg = readMessage()) {
            co_return processMessage(*msg);
        }
    }

    co_return false;
}

QCoro::Task<> DiscordIPCClient::changeClientId(const QString clientId)
{
    if(std::exchange(m_clientId, clientId) == clientId) {
        co_return;
    }

    ++m_connectGeneration;

    if(m_socket.state() != QLocalSocket::UnconnectedState || m_handshakeCompleted || m_connectInProgress) {
        co_await disconnectFromDiscord();
        co_await connectToDiscord();
    }
}

void DiscordIPCClient::sendMessage(const QJsonObject& packet, int opCode)
{
    const QByteArray payload = QJsonDocument(packet).toJson(QJsonDocument::Compact);
    qCDebug(DISCORD) << "→ SEND" << opCode << payload.length() << packet;

    MessageHeader header;
    header.opcode = static_cast<uint32_t>(opCode);
    header.length = static_cast<uint32_t>(payload.length());

    m_stream.writeRawData(reinterpret_cast<const char*>(&header), sizeof(MessageHeader));
    m_stream.writeRawData(payload.constData(), payload.size());
    m_socket.flush();
}

void DiscordIPCClient::setError(const QString& error)
{
    m_error = error;
    qCWarning(DISCORD) << m_error;
}

std::optional<DiscordMessage> DiscordIPCClient::readMessage()
{
    if(!hasCompleteMessage()) {
        return {};
    }

    MessageHeader header;
    m_stream.readRawData(reinterpret_cast<char*>(&header), sizeof(MessageHeader));

    if(m_socket.bytesAvailable() < static_cast<qint64>(header.length)) {
        return {};
    }

    QByteArray data(header.length, Qt::Uninitialized);
    m_stream.readRawData(data.data(), header.length);

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if(err.error != QJsonParseError::NoError) {
        qCWarning(DISCORD) << "Failed to parse message:" << err.errorString() << "\nData:" << data;
        return {};
    }

    DiscordMessage result{doc.object(), static_cast<int>(header.opcode)};
    qCDebug(DISCORD) << "← RECV" << header.opcode << header.length << result.json();
    return result;
}

bool DiscordIPCClient::hasCompleteMessage()
{
    if(m_socket.bytesAvailable() < static_cast<qint64>(sizeof(MessageHeader))) {
        return false;
    }

    const QByteArray headerData = m_socket.peek(sizeof(MessageHeader));
    if(headerData.size() < static_cast<qsizetype>(sizeof(MessageHeader))) {
        return false;
    }

    MessageHeader header;
    memcpy(&header, headerData.constData(), sizeof(MessageHeader));

    return std::cmp_greater_equal(m_socket.bytesAvailable(), sizeof(MessageHeader) + header.length);
}

QCoro::Task<bool> DiscordIPCClient::waitForReadyRead()
{
    co_return co_await qCoro(m_socket).waitForReadyRead(std::chrono::milliseconds{ReplyTimeout});
}

QCoro::Task<bool> DiscordIPCClient::startHandshake()
{
    if(m_handshakeCompleted) {
        co_return true;
    }

    const QJsonObject handshake{{u"v"_s, 1}, {u"client_id"_s, m_clientId}};
    sendMessage(handshake, Handshake);

    if(co_await waitForReadyRead()) {
        if(auto msg = readMessage()) {
            const auto json = msg->json();

            if(json.value("cmd"_L1).toString() == "DISPATCH"_L1 && json.value("evt"_L1).toString() == "READY"_L1) {
                m_handshakeCompleted = true;
                qInfo(DISCORD) << "Handshake successful, user:" << json["data"_L1]["user"_L1]["username"_L1].toString();
                co_return true;
            }
        }
    }

    setError(u"Handshake failed"_s);
    co_return false;
}

bool DiscordIPCClient::processMessage(const DiscordMessage& message)
{
    const auto json = message.json();
    if(json.isEmpty()) {
        qCDebug(DISCORD) << "Received empty message";
        return false;
    }

    if(json.value("cmd"_L1) == "ERROR"_L1) {
        const int errorCode        = json.value("code"_L1).toInt();
        const QString errorMessage = json.value("message"_L1).toString();

        setError(u"ERROR %1: %2"_s.arg(errorCode).arg(errorMessage));

        return false;
    }

    if(json.value("cmd"_L1) != "SET_ACTIVITY"_L1) {
        setError(u"Unexpected response: %1"_s.arg(json.value("cmd"_L1).toString()));

        return false;
    }

    return true;
}
} // namespace Fooyin::Discord
