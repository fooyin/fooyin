/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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
#include "discordpresencedata.h"

#include <utils/fypaths.h>

#include <QApplication>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTimer>
#include <QUuid>

Q_LOGGING_CATEGORY(DISCORD, "fy.discord")

using namespace Qt::StringLiterals;

namespace {
constexpr auto ReplyTimeout = 5000;

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
        case(QLocalSocket::ConnectionRefusedError):
            return u"Connection refused"_s;
        case(QLocalSocket::PeerClosedError):
            return u"Peer closed the connection"_s;
        case(QLocalSocket::ServerNotFoundError):
            return u"Server not found"_s;
        case(QLocalSocket::SocketAccessError):
            return u"Access error"_s;
        case(QLocalSocket::SocketResourceError):
            return u"Resource error"_s;
        case(QLocalSocket::SocketTimeoutError):
            return u"Socket timeout"_s;
        case(QLocalSocket::DatagramTooLargeError):
            return u"Datagram too large"_s;
        case(QLocalSocket::ConnectionError):
            return u"Network connection error"_s;
        case(QLocalSocket::UnsupportedSocketOperationError):
            return u"Unsupported operation"_s;
        case(QLocalSocket::UnknownSocketError):
            return u"Unknown error"_s;
        case(QLocalSocket::OperationError):
            return u"Operation error"_s;
        default:
            return u"Unrecognized error"_s;
    }
}

QString pipeLocation(int index)
{
#if defined(Q_OS_WIN)
    return uR"(\\?\pipe\discord-ipc-%1)"_s.arg(index);

#elif defined(Q_OS_MAC)
    return QDir::homePath() + u"/Library/Application Support/discord-ipc-%1"_s.arg(index);

#else
    QString runDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if(runDir.isEmpty()) {
        runDir = u"/tmp"_s;
    }
    return u"%1/discord-ipc-%2"_s.arg(runDir).arg(index);
#endif
}
} // namespace

namespace Fooyin::Discord {
DiscordIPCClient::DiscordIPCClient(QObject* parent)
    : QObject{parent}
    , m_stream{&m_socket}
    , m_pipeIndex{0}
    , m_handshakeCompleted{false}
    , m_connected{false}
{
    m_stream.setByteOrder(QDataStream::LittleEndian);
    m_stream.setVersion(QDataStream::Qt_6_0);

    QObject::connect(&m_socket, &QLocalSocket::errorOccurred, this, [](QLocalSocket::LocalSocketError error) {
        qCWarning(DISCORD) << "Socket:" << socketErrorToString(error);
    });

    QObject::connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        qCDebug(DISCORD) << "Disconnected from Discord IPC";
        m_connected = false;
        disconnectFromDiscord();
    });

    QObject::connect(&m_socket, &QLocalSocket::readyRead, this, &DiscordIPCClient::readAndProcessMessages);
}

bool DiscordIPCClient::isConnected() const
{
    return m_connected;
}

QString DiscordIPCClient::status() const
{
    return m_status;
}

void DiscordIPCClient::connectToDiscord()
{
    if(m_connected) {
        return;
    }

    if(m_clientId.isEmpty()) {
        m_connectionError = u"No Client ID"_s;
        qCDebug(DISCORD) << m_connectionError;
        return;
    }

    m_pipeIndex = 0;

    connectToPipe();
}

void DiscordIPCClient::disconnectFromDiscord()
{
    if(m_connected) {
        m_socket.disconnectFromServer();
    }
    m_connected          = false;
    m_handshakeCompleted = false;
    m_replies.clear();
}

void DiscordIPCClient::updateActivity(const PresenceData& data)
{
    if(!m_connected || !m_handshakeCompleted) {
        return;
    }

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
    const QJsonObject activityMessage{{u"cmd"_s, u"SET_ACTIVITY"_s},
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

    m_replies.emplace_back(nonce);
    sendMessage(activityMessage, Frame);

    startTimeout(nonce);
}

void DiscordIPCClient::clearActivity()
{
    if(!m_connected || !m_handshakeCompleted) {
        return;
    }

    const QString nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject activityMessage{
        {u"cmd"_s, u"SET_ACTIVITY"_s},
        {u"args"_s, QJsonObject{{u"pid"_s, QCoreApplication::applicationPid()}, {u"activity"_s, QJsonValue::Null}}},
        {u"nonce"_s, nonce}};

    m_replies.emplace_back(nonce);
    sendMessage(activityMessage, Frame);
}

void DiscordIPCClient::changeClientId(const QString& clientId)
{
    if(std::exchange(m_clientId, clientId) == clientId) {
        return;
    }

    if(m_socket.isValid() || m_handshakeCompleted) {
        disconnectFromDiscord();
        connectToDiscord();
    }
}

void DiscordIPCClient::connectToPipe()
{
    if(m_pipeIndex >= 10) {
        m_connectionError = u"No IPC pipe available"_s;
        qCWarning(DISCORD) << m_connectionError;
        disconnectFromDiscord();
        return;
    }

    const QString pipeName = pipeLocation(m_pipeIndex);
    qCDebug(DISCORD) << "Attempting to connect to" << pipeName;

    QObject::connect(
        &m_socket, &QLocalSocket::connected, this,
        [this, pipeName]() {
            m_connected = true;
            qCDebug(DISCORD) << "Connected to" << pipeName;
            startHandshake();
        },
        Qt::SingleShotConnection);

    QObject::connect(
        &m_socket, &QLocalSocket::errorOccurred, this,
        [this](QLocalSocket::LocalSocketError) {
            m_socket.disconnectFromServer();
            m_pipeIndex++;
            connectToPipe();
        },
        Qt::SingleShotConnection);

    m_socket.connectToServer(pipeName);
}

void DiscordIPCClient::sendMessage(const QJsonObject& packet, int opCode)
{
    const QByteArray payload = QJsonDocument(packet).toJson(QJsonDocument::Compact);
    qCDebug(DISCORD) << "SEND:" << opCode << payload.length() << packet;

    MessageHeader header;
    header.opcode = static_cast<uint32_t>(opCode);
    header.length = static_cast<uint32_t>(payload.length());

    m_stream.writeRawData(reinterpret_cast<const char*>(&header), sizeof(MessageHeader));
    m_stream.writeRawData(payload.constData(), payload.size());
    m_socket.flush();
}

std::optional<DiscordMessage> DiscordIPCClient::readMessage()
{
    if(m_socket.bytesAvailable() < static_cast<qint64>(sizeof(MessageHeader))) {
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
    qCDebug(DISCORD) << "RECV:" << header.opcode << header.length << result.json();
    return result;
}

void DiscordIPCClient::readAndProcessMessages()
{
    while(m_socket.bytesAvailable()) {
        if(auto message = readMessage()) {
            processMessage(*message);
        }
    }
}

void DiscordIPCClient::startTimeout(const QString& nonce)
{
    QTimer::singleShot(ReplyTimeout, this, [this, nonce]() {
        const auto replyIt = std::ranges::find(m_replies, nonce);
        if(replyIt != m_replies.cend()) {
            m_connectionError = u"Response timeout"_s;
            qCWarning(DISCORD) << m_connectionError;

            m_replies.erase(replyIt);
        }
    });
}

void DiscordIPCClient::startHandshake()
{
    if(m_handshakeCompleted) {
        return;
    }

    const QJsonObject handshake{{u"v"_s, 1}, {u"client_id"_s, m_clientId}};
    const QString nonce = u"HANDSHAKE"_s;
    m_replies.emplace_back(nonce);

    sendMessage(handshake, Handshake);

    startTimeout(nonce);
}

void DiscordIPCClient::processMessage(const DiscordMessage& message)
{
    const auto json = message.json();
    if(json.isEmpty()) {
        qCDebug(DISCORD) << "Received empty message";
        return;
    }

    const QString cmd   = json.value("cmd"_L1).toString();
    const QString nonce = json.value("nonce"_L1).toString();

    if(cmd == "DISPATCH"_L1 && json.value("evt"_L1).toString() == "READY"_L1) {
        // Handshake
        m_handshakeCompleted = true;
        qInfo(DISCORD) << "Handshake successful, user:" << json["data"_L1]["user"_L1]["username"_L1].toString();

        std::erase(m_replies, u"HANDSHAKE"_s);

        emit ready();
        return;
    }

    if(std::erase(m_replies, nonce)) {
        if(json.value("cmd"_L1) == "ERROR"_L1) {
            const int errorCode        = json.value("code"_L1).toInt();
            const QString errorMessage = json.value("message"_L1).toString();

            m_connectionError = u"ERROR %1: %2"_s.arg(errorCode).arg(errorMessage);
            qCWarning(DISCORD) << m_connectionError;

            return;
        }

        if(json.value("cmd"_L1) != "SET_ACTIVITY"_L1 || json.value("nonce"_L1) != nonce) {
            m_connectionError = u"Unexpected response: %1"_s.arg(json.value("cmd"_L1).toString());
            qCWarning(DISCORD) << m_connectionError << json;
        }
    }
    else {
        qCDebug(DISCORD) << "No reply found for nonce:" << nonce;
    }
}
} // namespace Fooyin::Discord
