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

#include "lyricsource.h"

#include <utils/utils.h>

#include <QIODevice>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QStringDecoder>

Q_LOGGING_CATEGORY(LYRICS, "fy.lyrics")

namespace Fooyin::Lyrics {
LyricSource::LyricSource(NetworkAccessManager* network, SettingsManager* settings, int index, bool enabled,
                         QObject* parent)
    : QObject{parent}
    , m_network{network}
    , m_settings{settings}
    , m_index{index}
    , m_enabled{enabled}
{ }

int LyricSource::index() const
{
    return m_index;
}

bool LyricSource::enabled() const
{
    return m_enabled;
}

bool LyricSource::isLocal() const
{
    return false;
}

void LyricSource::setIndex(int index)
{
    m_index = index;
}

void LyricSource::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

NetworkAccessManager* LyricSource::network() const
{
    return m_network;
}

SettingsManager* LyricSource::settings() const
{
    return m_settings;
}

QByteArray LyricSource::toUtf8(QIODevice* file)
{
    const QByteArray data = file->readAll();
    if(data.isEmpty()) {
        return {};
    }

    QStringDecoder toUtf16;

    auto encoding = QStringConverter::encodingForData(data);
    if(encoding) {
        toUtf16 = QStringDecoder{encoding.value()};
    }
    else {
        const auto encodingName = Utils::detectEncoding(data);
        if(encodingName.isEmpty()) {
            return {};
        }

        encoding = QStringConverter::encodingForName(encodingName.constData());
        if(encoding) {
            toUtf16 = QStringDecoder{encoding.value()};
        }
        else {
            toUtf16 = QStringDecoder{encodingName.constData()};
        }
    }

    if(!toUtf16.isValid()) {
        toUtf16 = QStringDecoder{QStringConverter::Utf8};
    }

    QString string = toUtf16(data);
    string.replace(QLatin1String{"\n\n"}, QLatin1String{"\n"});
    string.replace(u'\r', u'\n');

    return string.toUtf8();
}

QString LyricSource::encode(const QString& str)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(str));
}

bool LyricSource::getJsonFromReply(QNetworkReply* reply, QJsonObject* obj)
{
    bool success{false};

    if(reply->error() == QNetworkReply::NoError) {
        if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            success = true;
        }
        else {
            qCDebug(LYRICS) << QStringLiteral("Received HTTP code %1")
                                   .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
    }
    else {
        qCDebug(LYRICS) << QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }

    if(reply->error() == QNetworkReply::NoError || reply->error() >= 200) {
        const QByteArray data = reply->readAll();
        if(data.isEmpty() || !extractJsonObj(data, obj)) {
            success = false;
        }
    }

    return success;
}

bool LyricSource::extractJsonObj(const QByteArray& data, QJsonObject* obj)
{
    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(data, &error);

    if(error.error != QJsonParseError::NoError) {
        qCInfo(LYRICS) << error.errorString();
        return false;
    }

    if(json.isObject()) {
        *obj = json.object();
    }

    return true;
}

QNetworkReply* LyricSource::reply() const
{
    return m_reply;
}

void LyricSource::setReply(QNetworkReply* reply)
{
    m_reply = reply;
}

void LyricSource::resetReply()
{
    if(m_reply) {
        QObject::disconnect(m_reply);
        m_reply->deleteLater();
    }
}
} // namespace Fooyin::Lyrics
