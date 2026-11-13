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

#include <core/player/playbacklisteningtracker.h>

#include <core/constants.h>

#include <QDateTime>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString propertyValue(const Track::ExtraProperties& properties, QLatin1StringView key)
{
    return properties.value(QString{key});
}

QString normalisedRemoteUri(const QString& uri)
{
    const QUrl url{uri};
    if(!url.isValid() || url.scheme().isEmpty()) {
        return uri.trimmed();
    }
    return url.adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
}

PlaybackSource playbackSourceForTrack(const Track& track)
{
    PlaybackSource source;

    if(!track.isRemote()) {
        source.kind        = PlaybackSourceKind::LocalTrack;
        source.displayName = track.effectiveTitle();
        source.originalUri = track.filepath();

        const QString segment = u"%1\x1f%2\x1f%3"_s.arg(track.subsong()).arg(track.offset()).arg(track.duration());
        source.stableKey      = track.hash().isEmpty() ? u"local:path:%1\x1f%2"_s.arg(track.uniqueFilepath(), segment)
                                                       : u"local:hash:%1\x1f%2"_s.arg(track.hash(), segment);
        return source;
    }

    source.kind = PlaybackSourceKind::RemoteStream;

    const auto properties    = track.extraProperties();
    const QString provider   = propertyValue(properties, QLatin1StringView{Constants::PlaybackSource::Provider});
    const QString externalId = propertyValue(properties, QLatin1StringView{Constants::PlaybackSource::ExternalId});
    source.displayName       = track.effectiveTitle();

    source.originalUri = propertyValue(properties, QLatin1StringView{Constants::PlaybackSource::OriginalUri});
    if(source.originalUri.isEmpty()) {
        source.originalUri = track.filepath();
    }

    const QString providerKey = provider.trimmed().toCaseFolded();
    source.stableKey          = !providerKey.isEmpty() && !externalId.isEmpty()
                                  ? u"remote:provider:%1:%2"_s.arg(providerKey, externalId.trimmed())
                                  : u"remote:url:%1"_s.arg(normalisedRemoteUri(source.originalUri));
    return source;
}

} // namespace

PlaybackListeningTracker::PlaybackListeningTracker(QObject* parent)
    : QObject{parent}
{ }

std::optional<ListeningSession> PlaybackListeningTracker::currentSession() const
{
    return m_session;
}

uint64_t PlaybackListeningTracker::listeningThreshold(uint64_t durationMs, double fraction, uint64_t timeMs)
{
    if(durationMs == 0) {
        return std::max<uint64_t>(30000, timeMs);
    }
    const auto percentageMs = static_cast<uint64_t>(static_cast<double>(durationMs > 200 ? durationMs - 200 : 0)
                                                    * std::clamp(fraction, 0.0, 1.0));
    return std::min(percentageMs, timeMs);
}

void PlaybackListeningTracker::start(uint64_t itemId, const Track& track)
{
    finish();
    if(!track.isValid()) {
        return;
    }
    m_session = ListeningSession{.itemId    = itemId,
                                 .source    = playbackSourceForTrack(track),
                                 .track     = track,
                                 .startedAt = QDateTime::currentMSecsSinceEpoch()};
    Q_EMIT sessionStarted(*m_session);
}

void PlaybackListeningTracker::updateMetadata(const Track& track)
{
    if(!m_session || !track.sameIdentityAs(m_session->track) || track.sameDataAs(m_session->track)) {
        return;
    }
    m_session->track = track;
    Q_EMIT metadataChanged(*m_session);
}

void PlaybackListeningTracker::addListeningTime(uint64_t deltaMs)
{
    if(!m_session || deltaMs == 0) {
        return;
    }
    m_session->listeningTimeMs += deltaMs;
    Q_EMIT listeningTimeChanged(m_session->itemId, deltaMs);
}

void PlaybackListeningTracker::finish()
{
    if(!m_session) {
        return;
    }
    const auto finished = std::exchange(m_session, std::nullopt);
    Q_EMIT sessionEnded(*finished);
}
} // namespace Fooyin
