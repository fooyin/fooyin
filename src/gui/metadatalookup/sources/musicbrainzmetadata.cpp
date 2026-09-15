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

#include "musicbrainzmetadata.h"

#include <core/network/networkutils.h>
#include <utils/settings/settingsmanager.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimerEvent>
#include <QUrlQuery>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(METADATA_LOOKUP, "fy.metadatalookup")

constexpr auto ResultLimit       = "MetadataLookup/SearchResultLimit"_L1;
constexpr auto RequestIntervalMs = 1000;
constexpr auto MaxRetries        = 3;

namespace Fooyin {
namespace {
QStringList stringArray(const QJsonValue& value)
{
    QStringList result;

    const auto entries = value.toArray();
    for(const auto& entry : entries) {
        const QString text = entry.isObject() ? entry.toObject().value("name"_L1).toString() : entry.toString();
        if(!text.isEmpty() && !result.contains(text)) {
            result.push_back(text);
        }
    }

    return result;
}

std::vector<ArtistCredit> parseArtistCredit(const QJsonValue& value)
{
    std::vector<ArtistCredit> result;

    const auto items = value.toArray();
    for(const auto& item : items) {
        const QJsonObject credit = item.toObject();
        const QJsonObject artist = credit.value("artist"_L1).toObject();

        ArtistCredit parsed;
        parsed.name       = credit.value("name"_L1).toString(artist.value("name"_L1).toString());
        parsed.joinPhrase = credit.value("joinphrase"_L1).toString();
        parsed.id         = artist.value("id"_L1).toString();

        if(!parsed.name.isEmpty() || !parsed.id.isEmpty()) {
            result.push_back(std::move(parsed));
        }
    }

    return result;
}

void parseLabelInfo(const QJsonValue& value, ReleaseSummary& summary)
{
    const auto items = value.toArray();
    for(const auto& item : items) {
        const QJsonObject info = item.toObject();
        const QString catalog  = info.value("catalog-number"_L1).toString();
        const QString label    = info.value("label"_L1).toObject().value("name"_L1).toString();

        if(!catalog.isEmpty() && !summary.catalogNumbers.contains(catalog)) {
            summary.catalogNumbers.push_back(catalog);
        }
        if(!label.isEmpty() && !summary.labels.contains(label)) {
            summary.labels.push_back(label);
        }
    }
}

void parseReleaseGroup(const QJsonValue& value, ReleaseSummary& summary)
{
    const QJsonObject group      = value.toObject();
    const QString releaseGroupId = group.value("id"_L1).toString();

    if(!releaseGroupId.isEmpty()) {
        summary.identifiers.emplace(u"MUSICBRAINZ_RELEASEGROUPID"_s, QStringList{releaseGroupId});
    }

    summary.originalDate = group.value("first-release-date"_L1).toString();

    const QString primary = group.value("primary-type"_L1).toString();
    if(!primary.isEmpty()) {
        summary.releaseTypes.push_back(primary);
    }

    const auto types = stringArray(group.value("secondary-types"_L1));
    for(const QString& type : types) {
        if(!summary.releaseTypes.contains(type)) {
            summary.releaseTypes.push_back(type);
        }
    }
}

ReleaseSummary parseSummary(const QJsonObject& object)
{
    ReleaseSummary result;
    result.sourceId       = u"musicbrainz"_s;
    result.id             = object.value("id"_L1).toString();
    result.title          = object.value("title"_L1).toString();
    result.artistCredit   = parseArtistCredit(object.value("artist-credit"_L1));
    result.date           = object.value("date"_L1).toString();
    result.country        = object.value("country"_L1).toString();
    result.status         = object.value("status"_L1).toString();
    result.disambiguation = object.value("disambiguation"_L1).toString();
    result.barcode        = object.value("barcode"_L1).toString();

    parseLabelInfo(object.value("label-info"_L1), result);
    parseReleaseGroup(object.value("release-group"_L1), result);

    if(!result.id.isEmpty()) {
        result.identifiers.emplace(u"MUSICBRAINZ_ALBUMID"_s, QStringList{result.id});
    }

    const QStringList albumArtistIds = artistCreditIds(result.artistCredit);
    if(!albumArtistIds.isEmpty()) {
        result.identifiers.emplace(u"MUSICBRAINZ_ALBUMARTISTID"_s, albumArtistIds);
    }

    const auto items = object.value("media"_L1).toArray();
    result.discCount = static_cast<int>(items.size());
    for(const auto& item : items) {
        const QJsonObject medium = item.toObject();
        const QString format     = medium.value("format"_L1).toString();
        if(!format.isEmpty() && !result.formats.contains(format)) {
            result.formats.push_back(format);
        }
    }

    return result;
}

bool parseDocument(const QByteArray& data, QJsonObject& object, QString& error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if(parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = parseError.error != QJsonParseError::NoError ? parseError.errorString() : u"Invalid JSON object"_s;
        return false;
    }

    object = document.object();

    if(object.contains("error"_L1)) {
        error = object.value("error"_L1).toString();
        if(error.isEmpty()) {
            error = object.value("message"_L1).toString(u"MusicBrainz returned an error"_s);
        }
        return false;
    }

    return true;
}
} // namespace

QString escapeQueryValue(const QString& value)
{
    QString escaped;
    escaped.reserve(value.size() * 2);

    static const QString special = uR"(+-!(){}[]^"~*?:\/|&)"_s;

    const auto chars = value.simplified();
    for(const QChar character : chars) {
        if(special.contains(character)) {
            escaped += u'\\';
        }
        escaped += character;
    }

    return escaped;
}

MusicBrainzMetadata::MusicBrainzMetadata(QNetworkAccessManager* network, QObject* parent)
    : MetadataLookupSource{network, parent}
    , m_generation{0}
    , m_busy{false}
{ }

MusicBrainzMetadata::~MusicBrainzMetadata()
{
    cancel();
}

QUrl buildReleaseUrl(const QString& releaseId)
{
    QUrl url{u"https://musicbrainz.org/ws/2/release/%1"_s.arg(releaseId)};

    QUrlQuery query;
    query.addQueryItem(u"fmt"_s, u"json"_s);
    query.addQueryItem(u"inc"_s, u"recordings+artist-credits+release-groups+labels+discids+isrcs+genres"_s);
    url.setQuery(query);

    return url;
}

QString MusicBrainzMetadata::id() const
{
    return u"musicbrainz"_s;
}

QString MusicBrainzMetadata::name() const
{
    return tr("MusicBrainz");
}

std::vector<LookupMode> MusicBrainzMetadata::supportedModes() const
{
    return {LookupMode::ArtistAlbum, LookupMode::DiscToc, LookupMode::ReleaseId, LookupMode::ReleaseGroupId};
}

void MusicBrainzMetadata::search(const LookupQuery& query)
{
    cancel();

    const bool empty = query.mode == LookupMode::ArtistAlbum
                         ? query.artist.simplified().isEmpty() && query.album.simplified().isEmpty()
                     : query.mode == LookupMode::DiscToc ? query.discToc.simplified().isEmpty()
                                                         : query.identifier.simplified().isEmpty();
    if(empty) {
        Q_EMIT failed(query.mode == LookupMode::ArtistAlbum ? tr("Enter an artist or album.")
                      : query.mode == LookupMode::DiscToc   ? tr("Enter a disc TOC.")
                                                            : tr("Enter a MusicBrainz ID."));
        return;
    }

    const int limit = std::clamp(m_settings.value(ResultLimit, 25).toInt(), 1, 100);
    enqueue(
        {.type = OperationType::Search, .url = buildSearchUrl(query, limit), .generation = m_generation, .retries = 0});
}

void MusicBrainzMetadata::fetchRelease(const QString& releaseId)
{
    cancel();

    if(releaseId.isEmpty()) {
        Q_EMIT failed(tr("The selected result has no release identifier."));
        return;
    }

    if(const auto cached = m_releaseCache.find(releaseId); cached != m_releaseCache.cend()) {
        Q_EMIT releaseFetched(cached->second);
        return;
    }

    enqueue(
        {.type = OperationType::Release, .url = buildReleaseUrl(releaseId), .generation = m_generation, .retries = 0});
}

void MusicBrainzMetadata::cancel()
{
    ++m_generation;

    m_startTimer.stop();
    m_queue.clear();
    m_active.reset();

    if(m_reply) {
        QObject::disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    setBusy(false);
}

QString MusicBrainzMetadata::buildSearchExpression(const LookupQuery& query)
{
    QStringList clauses;

    if(query.mode == LookupMode::ArtistAlbum) {
        const QString album = query.album.simplified();
        if(!album.isEmpty()) {
            clauses.push_back(uR"(release:"%1")"_s.arg(escapeQueryValue(album)));
        }
        const QString artist = query.artist.simplified();
        if(!artist.isEmpty()) {
            clauses.push_back(uR"(artist:"%1")"_s.arg(escapeQueryValue(artist)));
        }
    }
    else if((query.mode == LookupMode::ReleaseId || query.mode == LookupMode::ReleaseGroupId)
            && !query.identifier.simplified().isEmpty()) {
        const auto field = query.mode == LookupMode::ReleaseId ? "reid"_L1 : "rgid"_L1;
        clauses.push_back(uR"(%1:"%2")"_s.arg(field, escapeQueryValue(query.identifier)));
    }

    return clauses.join(u" AND "_s);
}

QUrl MusicBrainzMetadata::buildSearchUrl(const LookupQuery& query, int limit)
{
    QUrl url{query.mode == LookupMode::DiscToc ? u"https://musicbrainz.org/ws/2/discid/-"_s
                                               : u"https://musicbrainz.org/ws/2/release/"_s};

    QUrlQuery urlQuery;
    if(query.mode == LookupMode::DiscToc) {
        urlQuery.addQueryItem(u"toc"_s, query.discToc.simplified());
        urlQuery.addQueryItem(u"inc"_s, u"artists"_s);
    }
    else {
        urlQuery.addQueryItem(u"query"_s, buildSearchExpression(query));
    }
    urlQuery.addQueryItem(u"fmt"_s, u"json"_s);
    urlQuery.addQueryItem(u"limit"_s, QString::number(std::clamp(limit, 1, 100)));
    url.setQuery(urlQuery);

    return url;
}

bool MusicBrainzMetadata::parseSearchResponse(const QByteArray& data, std::vector<ReleaseSummary>& releases,
                                              QString& error)
{
    releases.clear();

    QJsonObject object;
    if(!parseDocument(data, object, error)) {
        return false;
    }

    const auto values = object.value("releases"_L1).toArray();
    for(const auto& value : values) {
        ReleaseSummary summary = parseSummary(value.toObject());
        if(!summary.id.isEmpty()) {
            releases.push_back(std::move(summary));
        }
    }

    return true;
}

bool MusicBrainzMetadata::parseReleaseResponse(const QByteArray& data, Release& release, QString& error)
{
    QJsonObject object;
    if(!parseDocument(data, object, error)) {
        return false;
    }

    release         = {};
    release.summary = parseSummary(object);
    release.genres  = stringArray(object.value("genres"_L1));

    const auto values = object.value("media"_L1).toArray();
    for(const auto& mediumValue : values) {
        const QJsonObject mediumObject = mediumValue.toObject();

        ReleaseMedium medium;
        medium.position = mediumObject.value("position"_L1).toInt();
        medium.title    = mediumObject.value("title"_L1).toString();
        medium.format   = mediumObject.value("format"_L1).toString();

        const QJsonArray discs = mediumObject.value("discs"_L1).toArray();
        for(const auto& discValue : discs) {
            const QString discId = discValue.toObject().value("id"_L1).toString();
            if(!discId.isEmpty() && !medium.discIds.contains(discId)) {
                medium.discIds.push_back(discId);
            }
        }

        const QJsonArray tracks = mediumObject.value("tracks"_L1).toArray();
        for(const auto& trackValue : tracks) {
            const QJsonObject trackObject     = trackValue.toObject();
            const QJsonObject recordingObject = trackObject.value("recording"_L1).toObject();

            const QString releaseTrackId = trackObject.value("id"_L1).toString();
            const QString recordingId    = recordingObject.value("id"_L1).toString();

            ReleaseTrack track;
            track.title        = trackObject.value("title"_L1).toString(recordingObject.value("title"_L1).toString());
            track.artistCredit = parseArtistCredit(trackObject.value("artist-credit"_L1));

            if(track.artistCredit.empty()) {
                track.artistCredit = parseArtistCredit(recordingObject.value("artist-credit"_L1));
            }
            if(track.artistCredit.empty()) {
                track.artistCredit = release.summary.artistCredit;
            }
            if(!recordingId.isEmpty()) {
                track.identifiers.emplace(u"MUSICBRAINZ_TRACKID"_s, QStringList{recordingId});
            }
            if(!releaseTrackId.isEmpty()) {
                track.identifiers.emplace(u"MUSICBRAINZ_RELEASETRACKID"_s, QStringList{releaseTrackId});
            }

            const QStringList artistIds = artistCreditIds(track.artistCredit);
            if(!artistIds.isEmpty()) {
                track.identifiers.emplace(u"MUSICBRAINZ_ARTISTID"_s, artistIds);
            }
            track.isrcs          = stringArray(recordingObject.value("isrcs"_L1));
            track.genres         = stringArray(recordingObject.value("genres"_L1));
            track.mediumPosition = medium.position;
            track.position       = trackObject.value("position"_L1).toInt();
            track.number         = trackObject.value("number"_L1).toString();
            track.total          = static_cast<int>(tracks.size());
            track.durationMs     = trackObject.value("length"_L1).toInteger(-1);

            if(track.durationMs <= 0) {
                track.durationMs = recordingObject.value("length"_L1).toInteger(-1);
            }
            if(track.durationMs <= 0) {
                track.durationMs = -1;
            }
            if(!releaseTrackId.isEmpty() || !recordingId.isEmpty()) {
                medium.tracks.push_back(std::move(track));
            }
        }
        release.media.push_back(std::move(medium));
    }

    if(release.summary.id.isEmpty()) {
        error = u"MusicBrainz release response has no release ID"_s;
        return false;
    }

    return true;
}

void MusicBrainzMetadata::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_startTimer.timerId()) {
        m_startTimer.stop();
        startNext();
    }
    MetadataLookupSource::timerEvent(event);
}

void MusicBrainzMetadata::enqueue(Operation operation)
{
    m_queue.push_back(std::move(operation));
    setBusy(true);
    startNext();
}

void MusicBrainzMetadata::startNext()
{
    if(m_reply || m_active || m_startTimer.isActive() || m_queue.empty()) {
        if(m_queue.empty() && !m_reply && !m_active) {
            setBusy(false);
        }
        return;
    }

    const int elapsed = m_lastStarted.isValid() ? static_cast<int>(m_lastStarted.elapsed()) : RequestIntervalMs;
    if(elapsed < RequestIntervalMs) {
        m_startTimer.start(RequestIntervalMs - elapsed, this);
        return;
    }

    const Operation operation = std::move(m_queue.front());
    m_queue.pop_front();

    if(operation.generation != m_generation) {
        startNext();
        return;
    }

    startOperation(operation);
}

void MusicBrainzMetadata::startOperation(const Operation& operation)
{
    QNetworkRequest request = makeNetworkRequest(operation.url);
    request.setRawHeader("Accept", "application/json");

    m_active = operation;
    m_lastStarted.restart();

    qCDebug(METADATA_LOOKUP) << "Starting MusicBrainz request" << static_cast<int>(operation.type) << "generation"
                             << operation.generation;

    m_reply = network()->get(request);
    QObject::connect(m_reply, &QNetworkReply::finished, this, &MusicBrainzMetadata::handleReply);
}

void MusicBrainzMetadata::handleReply()
{
    if(!m_reply || !m_active) {
        return;
    }

    QNetworkReply* reply{m_reply};
    m_reply = nullptr;
    const Operation operation{*m_active};
    m_active.reset();
    QObject::disconnect(reply, nullptr, this, nullptr);

    const int status            = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString reason        = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    const QByteArray retryAfter = reply->rawHeader("Retry-After");
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText                 = reply->errorString();
    const QByteArray data                          = reply->readAll();

    reply->deleteLater();

    if(operation.generation != m_generation) {
        finishOperation();
        return;
    }

    if((status == 429 || status == 503) && operation.retries < MaxRetries) {
        retryOperation(operation, status, retryAfter);
        return;
    }

    if(networkError != QNetworkReply::NoError) {
        qCWarning(METADATA_LOOKUP) << "MusicBrainz request failed with HTTP" << status << networkErrorText;
        const QString error
            = status > 0 ? (reason.isEmpty() ? u"HTTP %1"_s.arg(status) : u"HTTP %1: %2"_s.arg(status).arg(reason))
                         : networkErrorText;
        Q_EMIT failed(tr("MusicBrainz request failed: %1").arg(error));
        finishOperation();
        return;
    }

    QString error;

    if(operation.type == OperationType::Search) {
        std::vector<ReleaseSummary> releases;
        if(parseSearchResponse(data, releases, error)) {
            Q_EMIT searchFinished(releases);
        }
        else {
            qCWarning(METADATA_LOOKUP) << "Could not parse MusicBrainz search response:" << error;
            Q_EMIT failed(tr("Could not read the MusicBrainz search response: %1").arg(error));
        }
    }
    else {
        Release release;
        if(parseReleaseResponse(data, release, error)) {
            m_releaseCache.insert_or_assign(release.summary.id, release);
            Q_EMIT releaseFetched(release);
        }
        else {
            qCWarning(METADATA_LOOKUP) << "Could not parse MusicBrainz release response:" << error;
            Q_EMIT failed(tr("Could not read the MusicBrainz release response: %1").arg(error));
        }
    }

    finishOperation();
}

void MusicBrainzMetadata::finishOperation()
{
    if(m_queue.empty()) {
        setBusy(false);
    }
    startNext();
}

void MusicBrainzMetadata::retryOperation(Operation operation, int statusCode, const QByteArray& retryAfter)
{
    ++operation.retries;

    bool validRetryAfter{false};
    const int serverDelay = retryAfter.toInt(&validRetryAfter) * 1000;
    const int backoff
        = validRetryAfter ? std::max(RequestIntervalMs, serverDelay) : RequestIntervalMs * (1 << operation.retries);
    qCWarning(METADATA_LOOKUP) << "MusicBrainz request throttled with HTTP" << statusCode << "retrying in" << backoff
                               << "ms";
    m_queue.push_front(std::move(operation));
    m_startTimer.start(backoff, this);
}

void MusicBrainzMetadata::setBusy(bool busy)
{
    if(std::exchange(m_busy, busy) != busy) {
        Q_EMIT busyChanged(busy);
    }
}
} // namespace Fooyin

#include "moc_musicbrainzmetadata.cpp"
