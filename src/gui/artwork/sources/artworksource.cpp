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

#include "artworksource.h"

#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QBuffer>
#include <QIODevice>
#include <QImageReader>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QStringDecoder>

Q_LOGGING_CATEGORY(ARTWORK, "fy.artwork")

using namespace Qt::StringLiterals;

constexpr auto MaxSize = 1024;

namespace {
QSize calculateScaledSize(const QSize& originalSize, int maxSize)
{
    int newWidth{0};
    int newHeight{0};

    if(originalSize.width() > originalSize.height()) {
        newWidth  = maxSize;
        newHeight = (maxSize * originalSize.height()) / originalSize.width();
    }
    else {
        newHeight = maxSize;
        newWidth  = (maxSize * originalSize.width()) / originalSize.height();
    }

    return {newWidth, newHeight};
}
} // namespace

namespace Fooyin {
ArtworkSource::ArtworkSource(NetworkAccessManager* network, SettingsManager* settings, int index, bool enabled,
                             QObject* parent)
    : QObject{parent}
    , m_network{network}
    , m_settings{settings}
    , m_index{index}
    , m_enabled{enabled}
{ }

int ArtworkSource::index() const
{
    return m_index;
}

bool ArtworkSource::enabled() const
{
    return m_enabled;
}

void ArtworkSource::setIndex(int index)
{
    m_index = index;
}

void ArtworkSource::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

QImage ArtworkSource::readImage(QByteArray data)
{
    QBuffer buffer{&data};
    const QMimeDatabase mimeDb;
    const auto mimeType   = mimeDb.mimeTypeForData(&buffer);
    const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

    QImageReader reader{&buffer, formatHint};

    if(!reader.canRead()) {
        qCDebug(ARTWORK) << "Failed to use format hint" << formatHint << "when trying to load embedded cover";

        reader.setFormat({});
        reader.setDevice(&buffer);
        if(!reader.canRead()) {
            qCDebug(ARTWORK) << "Failed to load embedded cover";
            return {};
        }
    }

    const auto size = reader.size();
    if(size.width() > MaxSize || size.height() > MaxSize) {
        const auto scaledSize = calculateScaledSize(size, MaxSize);
        reader.setScaledSize(scaledSize);
    }

    return reader.read();
}

QImage ArtworkSource::readImage(const QString& path, int requestedSize, const char* hintType)
{
    const QMimeDatabase mimeDb;
    const auto mimeType   = mimeDb.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

    QImageReader reader{path, formatHint};

    if(!reader.canRead()) {
        qCDebug(ARTWORK) << "Failed to use format hint" << formatHint << "when trying to load" << hintType << "cover";

        reader.setFormat({});
        reader.setFileName(path);
        if(!reader.canRead()) {
            qCDebug(ARTWORK) << "Failed to load" << hintType << "cover";
            return {};
        }
    }

    const auto size    = reader.size();
    const auto maxSize = requestedSize == 0 ? MaxSize : requestedSize;
    const auto dpr     = Utils::windowDpr();

    if(size.width() > maxSize || size.height() > maxSize || dpr > 1.0) {
        const auto scaledSize = calculateScaledSize(size, static_cast<int>(maxSize * dpr));
        reader.setScaledSize(scaledSize);
    }

    QImage image = reader.read();
    image.setDevicePixelRatio(dpr);

    return image;
}

NetworkAccessManager* ArtworkSource::network() const
{
    return m_network;
}

SettingsManager* ArtworkSource::settings() const
{
    return m_settings;
}

QString ArtworkSource::toUtf8(QIODevice* file)
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
    string.replace("\n\n"_L1, "\n"_L1);
    string.replace('\r'_L1, '\n'_L1);

    return string;
}

QString ArtworkSource::encode(const QString& str)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(str));
}

bool ArtworkSource::getJsonFromReply(QNetworkReply* reply, QJsonObject* obj)
{
    bool success{false};

    if(reply->error() == QNetworkReply::NoError) {
        if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            success = true;
        }
        else {
            qCDebug(ARTWORK) << u"Received HTTP code %1"_s.arg(
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
    }
    else {
        qCDebug(ARTWORK) << u"%1 (%2)"_s.arg(reply->errorString()).arg(reply->error());
    }

    if(reply->error() == QNetworkReply::NoError || reply->error() >= 200) {
        const QByteArray data = reply->readAll();
        if(data.isEmpty() || !extractJsonObj(data, obj)) {
            success = false;
        }
    }

    return success;
}

bool ArtworkSource::extractJsonObj(const QByteArray& data, QJsonObject* obj)
{
    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(data, &error);

    if(error.error != QJsonParseError::NoError) {
        qCInfo(ARTWORK) << error.errorString();
        return false;
    }

    if(json.isObject()) {
        *obj = json.object();
    }

    return true;
}

QNetworkReply* ArtworkSource::reply() const
{
    return m_reply;
}

void ArtworkSource::setReply(QNetworkReply* reply)
{
    m_reply = reply;
}

void ArtworkSource::resetReply()
{
    if(m_reply) {
        QObject::disconnect(m_reply);
        m_reply->deleteLater();
    }
}
} // namespace Fooyin
