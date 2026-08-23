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
 */

#include "discordartworkuploader.h"

#include <QCryptographicHash>
#include <QHttpMultiPart>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(DISCORD_ARTWORK, "fy.discord.artwork")

constexpr auto UploadTimeoutMs  = 15000;
constexpr auto MaxResponseBytes = 4096;
constexpr auto ExpiryMarginSecs = 5 * 60;

namespace {
QString retentionValue(int retentionHours)
{
    switch(retentionHours) {
        case 1:
        case 12:
        case 24:
        case 72:
            return QString::number(retentionHours) + u'h';
        default:
            return {};
    }
}
} // namespace

namespace Fooyin::Discord {
DiscordArtworkUploader::DiscordArtworkUploader(QNetworkAccessManager* network, QObject* parent)
    : QObject{parent}
    , m_network{network}
    , m_generation{0}
{ }

void DiscordArtworkUploader::upload(const QByteArray& artwork, const Config& config, QObject* context,
                                    Callback callback)
{
    cancel();

    if(!context || !callback || artwork.isEmpty() || !isValidEndpoint(config.endpoint)
       || retentionValue(config.retentionHours).isEmpty()) {
        return;
    }

    const QByteArray key = cacheKey(artwork, config);
    const QDateTime now  = QDateTime::currentDateTimeUtc();
    for(auto it = m_cache.begin(); it != m_cache.end();) {
        if(it->second.expiresAt <= now.addSecs(ExpiryMarginSecs)) {
            it = m_cache.erase(it);
        }
        else {
            ++it;
        }
    }

    if(const auto it = m_cache.find(key);
       it != m_cache.cend() && it->second.expiresAt > now.addSecs(ExpiryMarginSecs)) {
        callback(it->second);
        return;
    }

    auto* multipart = new QHttpMultiPart{QHttpMultiPart::FormDataType};

    const auto addTextPart = [multipart](const QByteArray& name, const QByteArray& value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       u"form-data; name=\"%1\""_s.arg(QString::fromLatin1(name)));
        part.setBody(value);
        multipart->append(part);
    };

    addTextPart("reqtype", "fileupload");
    addTextPart("time", retentionValue(config.retentionHours).toLatin1());

    QHttpPart artworkPart;
    artworkPart.setHeader(QNetworkRequest::ContentTypeHeader, u"image/jpeg"_s);
    artworkPart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        u"form-data; name=\"fileToUpload\"; filename=\"%1.jpg\""_s.arg(QString::fromLatin1(key.toHex())));
    artworkPart.setBody(artwork);

    multipart->append(artworkPart);

    QNetworkRequest request{config.endpoint};
    request.setTransferTimeout(UploadTimeoutMs);

    const uint64_t generation{m_generation};
    auto* reply = m_network->post(request, multipart);
    multipart->setParent(reply);
    m_reply = reply;

    const QPointer<QObject> callbackContext{context};
    QObject::connect(
        reply, &QNetworkReply::finished, this,
        [this, reply, callbackContext, callback = std::move(callback), key, config, generation]() {
            if(m_reply == reply) {
                m_reply.clear();
            }

            const QByteArray response = reply->readAll();
            const int status          = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool successful     = generation == m_generation && reply->error() == QNetworkReply::NoError
                                     && status >= 200 && status < 300 && response.size() <= MaxResponseBytes;

            bool uploaded{false};
            if(successful) {
                const QUrl url{QString::fromUtf8(response).trimmed()};
                if(isValidEndpoint(url)) {
                    const Result result{
                        .url       = url,
                        .expiresAt = QDateTime::currentDateTimeUtc().addSecs(config.retentionHours * 3600LL),
                    };
                    m_cache.emplace(key, result);

                    if(callbackContext) {
                        callback(result);
                    }

                    uploaded = true;
                }
                else {
                    qCWarning(DISCORD_ARTWORK) << "Artwork upload returned an invalid image URL";
                }
            }
            else if(generation == m_generation && reply->error() != QNetworkReply::OperationCanceledError) {
                qCWarning(DISCORD_ARTWORK)
                    << "Artwork upload failed:" << reply->errorString() << "HTTP status:" << status;
            }

            if(!uploaded && generation == m_generation && callbackContext) {
                callback({});
            }

            reply->deleteLater();
        });
}

void DiscordArtworkUploader::cancel()
{
    ++m_generation;

    if(m_reply) {
        m_reply->abort();
        m_reply.clear();
    }
}

void DiscordArtworkUploader::clearCache()
{
    m_cache.clear();
}

bool DiscordArtworkUploader::isValidEndpoint(const QUrl& endpoint)
{
    const QString scheme = endpoint.scheme().toLower();
    return endpoint.isValid() && (scheme == "http"_L1 || scheme == "https"_L1) && !endpoint.host().isEmpty()
        && endpoint.userInfo().isEmpty();
}

QByteArray DiscordArtworkUploader::cacheKey(const QByteArray& artwork, const Config& config)
{
    static const QByteArray Separator{1, '\0'};

    QCryptographicHash hash{QCryptographicHash::Sha256};
    hash.addData(config.endpoint.toEncoded());
    hash.addData(Separator);
    hash.addData(QByteArray::number(config.retentionHours));
    hash.addData(Separator);
    hash.addData(artwork);
    return hash.result();
}
} // namespace Fooyin::Discord

#include "moc_discordartworkuploader.cpp"
