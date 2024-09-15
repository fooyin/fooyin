/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "scrobblerauthsession.h"

#include <QApplication>
#include <QBuffer>
#include <QLoggingCategory>
#include <QNetworkProxy>
#include <QStyle>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(SCROBBLER_AUTH, "fy.scrobbler")

namespace {
QString getSuccessIcon()
{
    QBuffer buffer;
    if(buffer.open(QIODevice::WriteOnly)) {
        QApplication::style()->standardIcon(QStyle::SP_DialogOkButton).pixmap(40).toImage().save(&buffer, "PNG");
        return QString::fromUtf8(buffer.data().toBase64());
    }
    return {};
}

QString getErrorIcon()
{
    QBuffer buffer;
    if(buffer.open(QIODevice::WriteOnly)) {
        QApplication::style()->standardIcon(QStyle::SP_DialogAbortButton).pixmap(40).toImage().save(&buffer, "PNG");
        return QString::fromUtf8(buffer.data().toBase64());
    }
    return {};
}
} // namespace

namespace Fooyin::Scrobbler {
ScrobblerAuthSession::ScrobblerAuthSession(QObject* parent)
    : QObject{parent}
    , m_server{new QTcpServer(this)}
    , m_socket{nullptr}
    , m_tokenName{QStringLiteral("token")}
{
    m_server->setProxy(QNetworkProxy::NoProxy);
    if(!m_server->listen(QHostAddress::LocalHost)) {
        qCCritical(SCROBBLER_AUTH) << "Could not open port; callback won't work:" << m_server->errorString();
    }

    m_callbackUrl = QStringLiteral("http://localhost:%1").arg(m_server->serverPort());

    QObject::connect(m_server, &QTcpServer::newConnection, this, [this] {
        m_socket = m_server->nextPendingConnection();
        m_server->close();

        QObject::connect(m_socket, &QTcpSocket::readyRead, m_socket, [this] {
            requestData.append(m_socket->readAll());
            if(!m_socket->atEnd() && !requestData.endsWith("\r\n\r\n")) {
                qDebug(SCROBBLER_AUTH) << "Incomplete request; waiting for more data";
                return;
            }
            processCallback();
        });
        QObject::connect(m_socket, &QTcpSocket::disconnected, m_socket, &QTcpSocket::deleteLater);
        QObject::connect(m_socket, &QObject::destroyed, this, &QObject::deleteLater);
    });

    qCDebug(SCROBBLER_AUTH) << "Auth session constructed";
}

ScrobblerAuthSession::~ScrobblerAuthSession()
{
    m_server->close();
    m_server->deleteLater();
}

QString ScrobblerAuthSession::callbackUrl() const
{
    return m_callbackUrl;
}

void ScrobblerAuthSession::setAuthTokenName(const QString& name)
{
    m_tokenName = name;
}

void ScrobblerAuthSession::processCallback()
{
    const auto requestParts = requestData.split(u' ');
    if(requestParts.size() < 2 || requestParts.at(1).isEmpty()) {
        onError("400 Bad Request", tr("Malformed login callback."));
        return;
    }

    const QUrlQuery query{QUrl(QString::fromUtf8(requestParts.at(1))).query()};
    if(!query.hasQueryItem(m_tokenName)) {
        onError("400 Bad Request", tr("No login token in callback."));
        return;
    }

    qCDebug(SCROBBLER_AUTH) << "Found the token in callback";

    const auto msg
        = QStringLiteral("<div style='text-align:center;'>"
                         "<img src='data:image/png;base64,%1' alt='icon' width='40' height='40'/><br/>"
                         "<p>%2</p>"
                         "</div>\r\n")
              .arg(getSuccessIcon(), tr("The application has successfully logged in. This window can now be closed."));

    sendHttpResponse("200 OK", msg.toUtf8());

    emit tokenReceived(query.queryItemValue(m_tokenName));
}

void ScrobblerAuthSession::sendHttpResponse(const QByteArray& code, const QByteArray& msg)
{
    m_socket->write("HTTP/1.0 ");
    m_socket->write(code);
    m_socket->write("\r\n"
                    "Content-type: text/html;charset=UTF-8\r\n"
                    "\r\n\r\n");
    m_socket->write(msg);
    m_socket->write("\r\n");
}

void ScrobblerAuthSession::onError(const QByteArray& code, const QString& errorMsg)
{
    qCWarning(SCROBBLER_AUTH) << errorMsg;

    const auto msg = QStringLiteral("<div style='text-align:center;'>"
                                    "<img src='data:image/png;base64,%1' alt='icon' width='40' height='40'/><br/>"
                                    "<p>%2</p>"
                                    "</div>\r\n")
                         .arg(getErrorIcon(), errorMsg);

    sendHttpResponse(code, msg.toUtf8());
}
} // namespace Fooyin::Scrobbler
