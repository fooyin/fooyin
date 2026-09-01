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

#include "discordcoverartresolver.h"

#include <core/network/networkutils.h>

#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

using namespace Qt::StringLiterals;

constexpr auto RequestTimeoutMs  = 10000;
constexpr auto SuccessCacheSecs  = 24 * 60 * 60;
constexpr auto NegativeCacheSecs = 60 * 60;

namespace {
enum class MusicBrainzEntity : uint8_t
{
    Release = 0,
    ReleaseGroup,
};

QUrl coverArtArchiveUrl(const QString& musicBrainzId, MusicBrainzEntity entity, int size)
{
    const QUuid id{musicBrainzId};
    if(id.isNull() || (size != 500 && size != 1200)) {
        return {};
    }

    const auto entityName = entity == MusicBrainzEntity::Release ? u"release"_s : u"release-group"_s;
    return QUrl{u"https://coverartarchive.org/%1/%2/front-%3"_s.arg(entityName, id.toString(QUuid::WithoutBraces),
                                                                    QString::number(size))};
}

QString firstValidId(const Fooyin::Track& track, const QString& tag, MusicBrainzEntity entity)
{
    const auto tags = track.extraTag(tag);
    for(const QString& id : tags) {
        if(!coverArtArchiveUrl(id, entity, 1200).isEmpty()) {
            return id;
        }
    }
    return {};
}
} // namespace

namespace Fooyin::Discord {
DiscordCoverArtResolver::DiscordCoverArtResolver(QNetworkAccessManager* network, QObject* parent)
    : QObject{parent}
    , m_network{network}
    , m_candidateIndex{0}
    , m_generation{0}
{ }

void DiscordCoverArtResolver::resolve(const Track& track, QObject* context, Callback callback)
{
    cancel();

    if(!context || !callback) {
        return;
    }

    const std::vector<QUrl> candidates = candidateUrls(track);
    if(candidates.empty()) {
        callback({});
        return;
    }

    const QByteArray key = cacheKey(candidates);
    const QDateTime now  = QDateTime::currentDateTimeUtc();
    if(const auto it = m_cache.find(key); it != m_cache.cend() && it->second.expiresAt > now) {
        callback(it->second.url);
        return;
    }

    m_candidates     = candidates;
    m_cacheKey       = key;
    m_context        = context;
    m_callback       = std::move(callback);
    m_candidateIndex = 0;

    requestNext(m_generation);
}

void DiscordCoverArtResolver::cancel()
{
    ++m_generation;

    m_candidates.clear();
    m_cacheKey.clear();
    m_context.clear();
    m_callback       = {};
    m_candidateIndex = 0;

    if(m_reply) {
        m_reply->abort();
        m_reply.clear();
    }
}

void DiscordCoverArtResolver::clearCache()
{
    m_cache.clear();
}

std::vector<QUrl> DiscordCoverArtResolver::candidateUrls(const Track& track)
{
    std::vector<QUrl> candidates;

    const QString releaseId = firstValidId(track, u"MUSICBRAINZ_ALBUMID"_s, MusicBrainzEntity::Release);
    if(!releaseId.isEmpty()) {
        candidates.push_back(coverArtArchiveUrl(releaseId, MusicBrainzEntity::Release, 1200));
        candidates.push_back(coverArtArchiveUrl(releaseId, MusicBrainzEntity::Release, 500));
    }

    const QString releaseGroupId
        = firstValidId(track, u"MUSICBRAINZ_RELEASEGROUPID"_s, MusicBrainzEntity::ReleaseGroup);
    if(!releaseGroupId.isEmpty()) {
        candidates.push_back(coverArtArchiveUrl(releaseGroupId, MusicBrainzEntity::ReleaseGroup, 1200));
        candidates.push_back(coverArtArchiveUrl(releaseGroupId, MusicBrainzEntity::ReleaseGroup, 500));
    }

    return candidates;
}

QByteArray DiscordCoverArtResolver::cacheKey(const std::vector<QUrl>& candidates)
{
    static const QByteArray Separator{1, '\0'};

    QCryptographicHash hash{QCryptographicHash::Sha256};
    for(const QUrl& candidate : candidates) {
        hash.addData(candidate.toEncoded());
        hash.addData(Separator);
    }
    return hash.result();
}

void DiscordCoverArtResolver::requestNext(uint64_t generation)
{
    if(generation != m_generation) {
        return;
    }

    if(m_candidateIndex >= m_candidates.size()) {
        finish({}, generation);
        return;
    }

    const QUrl candidate    = m_candidates.at(m_candidateIndex++);
    QNetworkRequest request = makeNetworkRequest(candidate);
    request.setTransferTimeout(RequestTimeoutMs);

    auto* reply = m_network->head(request);
    m_reply     = reply;

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, candidate, generation] {
        if(m_reply == reply) {
            m_reply.clear();
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool found
            = generation == m_generation && reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
        reply->deleteLater();

        if(found) {
            finish(candidate, generation);
        }
        else if(generation == m_generation) {
            requestNext(generation);
        }
    });
}

void DiscordCoverArtResolver::finish(const std::optional<QUrl>& url, uint64_t generation)
{
    if(generation != m_generation) {
        return;
    }

    m_cache.emplace(m_cacheKey, CacheEntry{.url       = url,
                                           .expiresAt = QDateTime::currentDateTimeUtc().addSecs(
                                               url.has_value() ? SuccessCacheSecs : NegativeCacheSecs)});

    const QPointer<QObject> context{m_context};
    const Callback callback = std::move(m_callback);
    m_candidates.clear();
    m_cacheKey.clear();
    m_context.clear();
    m_candidateIndex = 0;

    if(context && callback) {
        callback(url);
    }
}
} // namespace Fooyin::Discord

#include "moc_discordcoverartresolver.cpp"
