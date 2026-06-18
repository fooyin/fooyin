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

#include "radiostreamresolver.h"

#include "radiobrowserutils.h"

#include <core/network/networkutils.h>
#include <core/playlist/playlistloader.h>

#include <QMetaObject>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <utility>

using namespace Qt::StringLiterals;

constexpr auto MaxPlaylistBytes = 1024 * 1024;
constexpr auto ProbeBytes       = 32 * 1024;

namespace {
QByteArray readReplyData(QNetworkReply* reply, qsizetype maxBytes)
{
    if(!reply || maxBytes <= 0 || !reply->isOpen() || !reply->isReadable()) {
        return {};
    }

    return reply->read(maxBytes);
}

QString contentType(QNetworkReply* reply)
{
    const QVariant header = reply->header(QNetworkRequest::ContentTypeHeader);
    if(header.isValid()) {
        return header.toString().section(u';', 0, 0).trimmed();
    }

    return QString::fromLatin1(reply->rawHeader("Content-Type")).section(u';', 0, 0).trimmed();
}

bool shouldFetchAsPlaylist(const QUrl& url)
{
    const QString path = url.path().toLower();
    return path.endsWith(u".m3u"_s) || path.endsWith(u".pls"_s);
}

bool isHlsUrl(const QUrl& url)
{
    return url.path().toLower().endsWith(u".m3u8"_s);
}

bool isPlaylistContentType(const QString& contentType)
{
    const QString type = contentType.toLower();
    return type.contains(u"mpegurl"_s) || type.contains(u"scpls"_s);
}

bool isAudioContentType(const QString& contentType)
{
    const QString type = contentType.toLower();
    return type.startsWith(u"audio/"_s) || type.startsWith(u"video/"_s) || type.contains(u"application/ogg"_s);
}

bool looksLikePlaylist(const QByteArray& data)
{
    const QByteArray trimmed = data.left(ProbeBytes).trimmed();
    const QByteArray lower   = trimmed.toLower();
    return trimmed.startsWith("#EXTM3U") || lower.startsWith("[playlist]") || lower.startsWith("file1=")
        || lower.contains("\nfile1=") || lower.contains("\r\nfile1=");
}

bool looksLikeHlsPlaylist(const QByteArray& data)
{
    const QByteArray probe = data.left(ProbeBytes).toUpper();
    return probe.startsWith("#EXTM3U") && probe.contains("#EXT-X-");
}

bool looksLikeBinaryStream(const QByteArray& data)
{
    if(data.startsWith("ID3") || data.startsWith("OggS") || data.startsWith("fLaC") || data.startsWith("\xFF\xFB")
       || data.startsWith("\xFF\xF3") || data.startsWith("\xFF\xF2") || data.startsWith("\xFF\xF1")
       || data.startsWith("\xFF\xF9")) {
        return true;
    }

    const qsizetype n = std::min<qsizetype>(data.size(), 512);
    if(n < 16) {
        return false;
    }

    for(qsizetype i{0}; i < n; ++i) {
        const auto value = static_cast<uint8_t>(data.at(i));
        if(value < 0x09 || (value > 0x0D && value < 0x20)) {
            return true;
        }
    }

    return false;
}
} // namespace

namespace Fooyin::RadioBrowser {
RadioStreamResolver::RadioStreamResolver(std::shared_ptr<NetworkAccessManager> network,
                                         std::shared_ptr<PlaylistLoader> playlistLoader, QObject* parent)
    : QObject{parent}
    , m_network{std::move(network)}
    , m_playlistLoader{std::move(playlistLoader)}
{ }

void RadioStreamResolver::resolve(int requestId, const QString& url)
{
    const QUrl streamUrl{url.trimmed()};
    if(!streamUrl.isValid() || streamUrl.scheme().isEmpty()) {
        Q_EMIT failed(requestId, url, tr("Invalid stream URL."));
        return;
    }

    if(isHlsUrl(streamUrl)) {
        Q_EMIT resolved(requestId, url, {url});
        return;
    }

    if(streamUrl.scheme() == u"http"_s || streamUrl.scheme() == u"https"_s) {
        fetchStreamInfo(requestId, streamUrl, shouldFetchAsPlaylist(streamUrl));
        return;
    }

    Q_EMIT resolved(requestId, url, {url});
}

void RadioStreamResolver::fetchStreamInfo(int requestId, const QUrl& url, bool forcePlaylist)
{
    QPointer<QNetworkReply> previousReply;
    if(const auto replyIt = m_replies.find(requestId); replyIt != m_replies.end()) {
        previousReply = replyIt->second;
        m_replies.erase(replyIt);
    }
    finishReply(previousReply);

    QNetworkRequest request = makeNetworkRequest(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    if(!forcePlaylist) {
        request.setRawHeader("Range", "bytes=0-1048575");
    }

    QNetworkReply* reply = m_network->get(request);
    m_replies.insert_or_assign(requestId, reply);
    m_pendingRequests.insert_or_assign(requestId,
                                       PendingRequest{.url = url, .data = {}, .forcePlaylist = forcePlaylist});

    const QPointer<QNetworkReply> guardedReply{reply};

    QObject::connect(reply, &QNetworkReply::metaDataChanged, this,
                     [this, requestId, guardedReply]() { handleMetadataChanged(requestId, guardedReply); });
    QObject::connect(reply, &QNetworkReply::readyRead, this,
                     [this, requestId, guardedReply]() { handleReadyRead(requestId, guardedReply); });
    QObject::connect(reply, &QNetworkReply::redirected, reply, [reply]() { Q_EMIT reply->redirectAllowed(); });
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, requestId, guardedReply, url]() { handleReply(requestId, guardedReply, url); });
}

void RadioStreamResolver::handleMetadataChanged(int requestId, const QPointer<QNetworkReply>& reply)
{
    if(!reply) {
        return;
    }

    const auto replyIt = m_replies.find(requestId);
    if(replyIt == m_replies.end() || replyIt->second != reply) {
        return;
    }

    auto pendingIt = m_pendingRequests.find(requestId);
    if(pendingIt == m_pendingRequests.end()) {
        return;
    }

    const QString type = contentType(reply);
    if(pendingIt->second.forcePlaylist || type.isEmpty() || isPlaylistContentType(type)) {
        return;
    }

    if(isAudioContentType(type)) {
        pendingIt->second.directStream = true;
    }
}

void RadioStreamResolver::handleReadyRead(int requestId, const QPointer<QNetworkReply>& reply)
{
    if(!reply) {
        return;
    }

    const auto replyIt = m_replies.find(requestId);
    if(replyIt == m_replies.end() || replyIt->second != reply) {
        return;
    }

    auto pendingIt = m_pendingRequests.find(requestId);
    if(pendingIt == m_pendingRequests.end()) {
        return;
    }

    auto& pending = pendingIt->second;
    pending.data += readReplyData(reply, MaxPlaylistBytes - pending.data.size());

    if(pending.directStream && !pending.data.isEmpty()) {
        const QUrl originalUrl{pending.url};
        queueCompleteDirectStream(requestId, reply, originalUrl);
        return;
    }

    if(looksLikeHlsPlaylist(pending.data)) {
        const QUrl pendingUrl = reply->url().isValid() ? reply->url() : pending.url;
        m_replies.erase(requestId);
        m_pendingRequests.erase(requestId);
        finishReply(reply);
        Q_EMIT resolved(requestId, pendingUrl.toString(), {pendingUrl.toString()});
        return;
    }

    if(!pending.forcePlaylist && !looksLikePlaylist(pending.data) && looksLikeBinaryStream(pending.data)) {
        const QUrl originalUrl = pending.url;
        queueCompleteDirectStream(requestId, reply, originalUrl);
        return;
    }

    if(pending.data.size() >= MaxPlaylistBytes && !looksLikePlaylist(pending.data)) {
        const QUrl originalUrl = pending.url;
        queueCompleteDirectStream(requestId, reply, originalUrl);
    }
}

void RadioStreamResolver::handleReply(int requestId, const QPointer<QNetworkReply>& reply, const QUrl& url)
{
    const auto replyIt = m_replies.find(requestId);
    if(replyIt == m_replies.end() || replyIt->second != reply) {
        return;
    }

    m_replies.erase(replyIt);

    const auto pendingIt = m_pendingRequests.find(requestId);
    if(pendingIt == m_pendingRequests.end()) {
        return;
    }

    PendingRequest pending = pendingIt->second;
    m_pendingRequests.erase(pendingIt);

    if(!reply) {
        return;
    }

    pending.data += readReplyData(reply, MaxPlaylistBytes - pending.data.size());

    const auto error          = reply->error();
    const QString errorString = reply->errorString();
    const QString type        = contentType(reply);
    const QUrl resolvedUrl    = reply->url().isValid() ? reply->url() : url;

    reply->deleteLater();

    if(error != QNetworkReply::NoError && error != QNetworkReply::OperationCanceledError) {
        Q_EMIT failed(requestId, url.toString(), Utils::displayNetworkError(errorString));
        return;
    }

    const bool playlist = pending.forcePlaylist || isPlaylistContentType(type) || looksLikePlaylist(pending.data);
    if(!playlist) {
        Q_EMIT resolved(requestId, url.toString(), {resolvedUrl.toString()});
        return;
    }

    if(looksLikeHlsPlaylist(pending.data)) {
        Q_EMIT resolved(requestId, url.toString(), {resolvedUrl.toString()});
        return;
    }

    const QStringList streamUrls = m_playlistLoader->resolveUrls(resolvedUrl, pending.data, type);
    if(streamUrls.empty()) {
        Q_EMIT failed(requestId, url.toString(), tr("Playlist did not contain any playable streams."));
        return;
    }

    QStringList uniqueStreamUrls{streamUrls};
    uniqueStreamUrls.removeDuplicates();
    Q_EMIT resolved(requestId, url.toString(), uniqueStreamUrls);
}

void RadioStreamResolver::finishReply(const QPointer<QNetworkReply>& reply) const
{
    if(!reply) {
        return;
    }

    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

void RadioStreamResolver::queueCompleteDirectStream(int requestId, const QPointer<QNetworkReply>& reply, QUrl url)
{
    QMetaObject::invokeMethod(
        this, [this, requestId, reply, url = std::move(url)]() { completeDirectStream(requestId, reply, url); },
        Qt::QueuedConnection);
}

void RadioStreamResolver::completeDirectStream(int requestId, const QPointer<QNetworkReply>& reply, const QUrl& url)
{
    if(!reply) {
        return;
    }

    const auto replyIt = m_replies.find(requestId);
    if(replyIt == m_replies.end() || replyIt->second != reply) {
        return;
    }

    const auto pendingIt = m_pendingRequests.find(requestId);
    if(pendingIt == m_pendingRequests.end()) {
        return;
    }

    const QUrl replyUrl    = reply->url();
    const QUrl resolvedUrl = replyUrl.isValid() ? replyUrl : url;

    m_replies.erase(replyIt);
    m_pendingRequests.erase(pendingIt);

    finishReply(reply);
    Q_EMIT resolved(requestId, url.toString(), {resolvedUrl.toString()});
}
} // namespace Fooyin::RadioBrowser
