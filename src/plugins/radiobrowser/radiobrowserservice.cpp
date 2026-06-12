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

#include "radiobrowserservice.h"

#include <core/network/networkutils.h>
#include <utils/settings/settingsmanager.h>

#include <QDnsLookup>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QUrl>
#include <QUrlQuery>

#include <array>
#include <ranges>

using namespace Qt::StringLiterals;

constexpr auto ApiSrvRecord = "_api._tcp.radio-browser.info"_L1;
constexpr auto Limit        = 100;

constexpr std::array FallbackApiBases{
    "https://de1.api.radio-browser.info",
    "https://de2.api.radio-browser.info",
    "https://nl1.api.radio-browser.info",
    "https://at1.api.radio-browser.info",
};

namespace Fooyin::RadioBrowser {
namespace {
QByteArray readReplyData(QNetworkReply* reply)
{
    if(!reply || !reply->isOpen() || !reply->isReadable()) {
        return {};
    }
    return reply->readAll();
}

QString normalisedApiBase(QString target)
{
    target = target.trimmed();
    while(target.endsWith(u'.')) {
        target.chop(1);
    }
    if(target.isEmpty()) {
        return {};
    }
    if(target.startsWith("http://"_L1) || target.startsWith("https://"_L1)) {
        return target;
    }
    return u"https://%1"_s.arg(target);
}

QString categoryPath(const RadioCategoryType type)
{
    switch(type) {
        case RadioCategoryType::Country:
            return u"countries"_s;
        case RadioCategoryType::Language:
            return u"languages"_s;
        case RadioCategoryType::Tag:
            return u"tags"_s;
        case RadioCategoryType::Codec:
            return u"codecs"_s;
    }

    return {};
}

QString stationString(const QJsonObject& object, const char* key)
{
    return object.value(QLatin1StringView{key}).toString().trimmed();
}

int stationInt(const QJsonObject& object, const char* key)
{
    return object.value(QLatin1StringView{key}).toVariant().toInt();
}

void appendEncodedQueryItem(QString& query, const QString& key, const QString& value)
{
    if(value.isEmpty()) {
        return;
    }
    if(!query.isEmpty()) {
        query.append(u'&');
    }
    query.append(key);
    query.append(u'=');
    query.append(QString::fromLatin1(QUrl::toPercentEncoding(value)));
}

RadioCategory categoryFromObject(const RadioCategoryType type, const QJsonObject& object)
{
    RadioCategory category;
    category.type         = type;
    category.name         = stationString(object, "name");
    category.value        = type == RadioCategoryType::Country ? stationString(object, "iso_3166_1") : category.name;
    category.stationCount = stationInt(object, "stationcount");

    if(type == RadioCategoryType::Country) {
        category.name = displayNameForCountry(category.name, category.value);
    }

    if(category.value.isEmpty()) {
        category.value = category.name;
    }

    return category;
}

RadioStation stationFromObject(const QJsonObject& object)
{
    RadioStation station;
    station.uuid              = stationString(object, "stationuuid");
    station.name              = stationString(object, "name");
    station.streamUrl         = stationString(object, "url");
    station.resolvedStreamUrl = stationString(object, "url_resolved");
    station.homepage          = stationString(object, "homepage");
    station.favicon           = stationString(object, "favicon");
    station.tags              = stationString(object, "tags");
    station.country           = stationString(object, "country");
    station.countryCode       = stationString(object, "countrycode");
    station.language          = stationString(object, "language");
    station.codec             = stationString(object, "codec");
    station.bitrate           = stationInt(object, "bitrate");
    station.clickCount        = stationInt(object, "clickcount");
    station.voteCount         = stationInt(object, "votes");
    station.online            = stationInt(object, "lastcheckok") != 0;
    station.hls               = stationInt(object, "hls") != 0;

    if(station.name.isEmpty()) {
        station.name = station.resolvedStreamUrl.isEmpty() ? station.streamUrl : station.resolvedStreamUrl;
    }

    return station;
}

RadioStationList stationsFromJsonArray(const QJsonArray& array)
{
    RadioStationList stations;
    stations.reserve(array.size());

    for(const auto& value : array) {
        if(!value.isObject()) {
            continue;
        }

        RadioStation station = stationFromObject(value.toObject());
        if(station.streamUrl.isEmpty() && station.resolvedStreamUrl.isEmpty()) {
            continue;
        }

        stations.emplace_back(std::move(station));
    }

    return stations;
}
} // namespace

RadioBrowserService::RadioBrowserService(std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings,
                                         QObject* parent)
    : QObject{parent}
    , m_network{std::move(network)}
    , m_settings{settings}
    , m_apiIndex{0}
    , m_apiDiscoveryFinished{false}
{ }

void RadioBrowserService::searchStations(const QString& query)
{
    RadioSearchRequest request;
    request.text  = query;
    request.order = u"clickcount"_s;
    searchStations(request);
}

void RadioBrowserService::searchStations(const RadioSearchRequest& request)
{
    if(m_reply) {
        QObject::disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
    }

    Q_EMIT searchStarted(request.text.trimmed());

    PendingApiRequest pending;
    pending.type                  = PendingRequestType::Station;
    pending.station.type          = StationRequestType::Search;
    pending.station.searchRequest = request;
    pending.station.limit         = request.limit;
    pending.station.offset        = request.offset;
    pending.station.apiIndex      = m_apiIndex;
    queueOrStart(pending);
}

void RadioBrowserService::fetchTopClicked(const int limit, const int offset)
{
    fetchStationsEndpoint(u"stations/topclick"_s, tr("Top clicked"), limit, offset);
}

void RadioBrowserService::fetchTopVoted(const int limit, const int offset)
{
    fetchStationsEndpoint(u"stations/topvote"_s, tr("Top voted"), limit, offset);
}

void RadioBrowserService::fetchRecentlyClicked(const int limit, const int offset)
{
    fetchStationsEndpoint(u"stations/lastclick"_s, tr("Recently clicked"), limit, offset);
}

void RadioBrowserService::fetchRecentlyChanged(const int limit, const int offset)
{
    fetchStationsEndpoint(u"stations/lastchange"_s, tr("Recently changed"), limit, offset);
}

void RadioBrowserService::fetchCategories(const RadioCategoryType type)
{
    if(const auto replyIt = m_categoryReplies.find(type); replyIt != m_categoryReplies.end() && replyIt->second) {
        QObject::disconnect(replyIt->second, nullptr, this, nullptr);
        replyIt->second->abort();
        replyIt->second->deleteLater();
        m_categoryReplies.erase(replyIt);
        m_categoryRequests.erase(type);
    }

    Q_EMIT categoriesStarted(type);

    PendingApiRequest pending;
    pending.type              = PendingRequestType::Category;
    pending.category.type     = type;
    pending.category.apiIndex = m_apiIndex;
    queueOrStart(pending);
}

void RadioBrowserService::fetchStationsByUuids(int requestId, const QStringList& uuids)
{
    QStringList cleanUuids;
    for(const QString& uuid : uuids) {
        const QString trimmed = uuid.trimmed();
        if(!trimmed.isEmpty() && !cleanUuids.contains(trimmed)) {
            cleanUuids.push_back(trimmed);
        }
    }

    if(cleanUuids.empty()) {
        Q_EMIT stationLookupFinished(requestId, {});
        return;
    }

    if(m_lookupReply) {
        QObject::disconnect(m_lookupReply, nullptr, this, nullptr);
        m_lookupReply->abort();
        m_lookupReply->deleteLater();
    }

    PendingApiRequest pending;
    pending.type             = PendingRequestType::Lookup;
    pending.lookup.requestId = requestId;
    pending.lookup.uuids     = cleanUuids;
    pending.lookup.apiIndex  = m_apiIndex;
    queueOrStart(pending);
}

void RadioBrowserService::fetchStationsByUrl(const int requestId, const QString& url)
{
    const QString trimmedUrl = url.trimmed();
    if(trimmedUrl.isEmpty()) {
        Q_EMIT stationLookupFinished(requestId, {});
        return;
    }

    if(m_lookupReply) {
        QObject::disconnect(m_lookupReply, nullptr, this, nullptr);
        m_lookupReply->abort();
        m_lookupReply->deleteLater();
    }

    PendingApiRequest pending;
    pending.type             = PendingRequestType::Lookup;
    pending.lookup.requestId = requestId;
    pending.lookup.url       = trimmedUrl;
    pending.lookup.apiIndex  = m_apiIndex;
    queueOrStart(pending);
}

void RadioBrowserService::reportClick(const QString& stationUuid)
{
    if(stationUuid.isEmpty()) {
        return;
    }

    if(m_clickReply) {
        QObject::disconnect(m_clickReply, nullptr, this, nullptr);
        m_clickReply->abort();
        m_clickReply->deleteLater();
    }

    PendingApiRequest pending;
    pending.type        = PendingRequestType::Click;
    pending.stationUuid = stationUuid;
    queueOrStart(pending);
}

void RadioBrowserService::startSearchRequest(const RadioSearchRequest& request, int apiIndex, int attempts)
{
    if(m_reply) {
        QObject::disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
    }

    const int normalisedIndex = (apiIndex + static_cast<int>(m_apiBases.size())) % static_cast<int>(m_apiBases.size());

    StationRequestContext context;
    context.type          = StationRequestType::Search;
    context.searchRequest = request;
    context.limit         = request.limit;
    context.offset        = request.offset;
    context.apiIndex      = normalisedIndex;
    context.attempts      = attempts;
    m_stationRequest      = std::move(context);

    QNetworkRequest networkRequest = makeNetworkRequest(buildSearchUrl(request, normalisedIndex));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);

    m_reply = m_network->get(networkRequest);
    QObject::connect(m_reply, &QNetworkReply::finished, this, &RadioBrowserService::handleReply);
}

void RadioBrowserService::startEndpointRequest(const QString& path, int limit, int offset, int apiIndex, int attempts)
{
    if(m_reply) {
        QObject::disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
    }

    const int normalisedIndex = (apiIndex + static_cast<int>(m_apiBases.size())) % static_cast<int>(m_apiBases.size());

    StationRequestContext context;
    context.type         = StationRequestType::Endpoint;
    context.endpointPath = path;
    context.limit        = limit;
    context.offset       = offset;
    context.apiIndex     = normalisedIndex;
    context.attempts     = attempts;
    m_stationRequest     = std::move(context);

    QNetworkRequest request = makeNetworkRequest(buildStationsEndpointUrl(path, limit, offset, normalisedIndex));
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);

    m_reply = m_network->get(request);
    QObject::connect(m_reply, &QNetworkReply::finished, this, &RadioBrowserService::handleReply);
}

void RadioBrowserService::startCategoryRequest(RadioCategoryType type, int apiIndex, int attempts)
{
    if(const auto replyIt = m_categoryReplies.find(type); replyIt != m_categoryReplies.end() && replyIt->second) {
        QObject::disconnect(replyIt->second, nullptr, this, nullptr);
        replyIt->second->abort();
        replyIt->second->deleteLater();
        m_categoryReplies.erase(replyIt);
    }

    const int normalisedIndex = (apiIndex + static_cast<int>(m_apiBases.size())) % static_cast<int>(m_apiBases.size());
    CategoryRequestContext context;
    context.type             = type;
    context.apiIndex         = normalisedIndex;
    context.attempts         = attempts;
    m_categoryRequests[type] = context;

    QNetworkRequest request = makeNetworkRequest(buildCategoryUrl(type, normalisedIndex));
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);

    const QPointer<QNetworkReply> reply = m_network->get(request);
    m_categoryReplies[type]             = reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, type]() { handleCategoriesReply(type); });
}

void RadioBrowserService::startStationLookupRequest(const StationLookupRequestContext& context)
{
    if(m_lookupReply) {
        QObject::disconnect(m_lookupReply, nullptr, this, nullptr);
        m_lookupReply->abort();
        m_lookupReply->deleteLater();
    }

    StationLookupRequestContext request{context};
    request.apiIndex = (request.apiIndex + static_cast<int>(m_apiBases.size())) % static_cast<int>(m_apiBases.size());
    m_lookupRequest  = request;

    QNetworkRequest networkRequest
        = makeNetworkRequest(request.url.isEmpty() ? buildStationLookupUrl(request.uuids, request.apiIndex)
                                                   : buildStationUrlLookupUrl(request.url, request.apiIndex));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);

    m_lookupReply = m_network->get(networkRequest);
    QObject::connect(m_lookupReply, &QNetworkReply::finished, this, &RadioBrowserService::handleStationLookupReply);
}

void RadioBrowserService::startClickRequest(const QString& stationUuid)
{
    const QNetworkRequest request = makeNetworkRequest(buildClickUrl(stationUuid));
    m_clickReply                  = m_network->get(request);
    QObject::connect(m_clickReply, &QNetworkReply::finished, this, [this]() {
        if(m_clickReply) {
            m_clickReply->deleteLater();
        }
    });
}

void RadioBrowserService::queueOrStart(const PendingApiRequest& request)
{
    if(m_apiDiscoveryFinished) {
        startPendingRequest(request);
        return;
    }

    switch(request.type) {
        case PendingRequestType::Station:
            m_pendingStationRequest = request.station;
            break;
        case PendingRequestType::Category:
            m_pendingCategoryRequests[request.category.type] = request.category;
            break;
        case PendingRequestType::Lookup:
            m_pendingLookupRequest = request.lookup;
            break;
        case PendingRequestType::Click:
            m_pendingClickRequest = request.stationUuid;
            break;
    }

    startApiServerLookup();
}

void RadioBrowserService::startPendingRequest(const PendingApiRequest& request)
{
    switch(request.type) {
        case PendingRequestType::Station:
            if(request.station.type == StationRequestType::Search) {
                startSearchRequest(request.station.searchRequest, request.station.apiIndex, request.station.attempts);
            }
            else if(request.station.type == StationRequestType::Endpoint) {
                startEndpointRequest(request.station.endpointPath, request.station.limit, request.station.offset,
                                     request.station.apiIndex, request.station.attempts);
            }
            break;
        case PendingRequestType::Category:
            startCategoryRequest(request.category.type, request.category.apiIndex, request.category.attempts);
            break;
        case PendingRequestType::Lookup:
            startStationLookupRequest(request.lookup);
            break;
        case PendingRequestType::Click:
            startClickRequest(request.stationUuid);
            break;
    }
}

void RadioBrowserService::startApiServerLookup()
{
    if(m_apiLookup) {
        return;
    }

    m_apiLookup = new QDnsLookup(QDnsLookup::SRV, ApiSrvRecord, this);
    QObject::connect(m_apiLookup, &QDnsLookup::finished, this, &RadioBrowserService::handleApiServerLookup);

    m_apiLookup->lookup();
}

void RadioBrowserService::handleApiServerLookup()
{
    QStringList apiBases;

    if(m_apiLookup && m_apiLookup->error() == QDnsLookup::NoError) {
        const auto records = m_apiLookup->serviceRecords();
        for(const QDnsServiceRecord& record : records) {
            const QString base = normalisedApiBase(record.target());
            if(!base.isEmpty() && !apiBases.contains(base)) {
                apiBases.push_back(base);
            }
        }
    }

    if(apiBases.empty()) {
        for(const char* fallback : FallbackApiBases) {
            apiBases.push_back(QString::fromLatin1(fallback));
        }
    }

    std::ranges::shuffle(apiBases, *QRandomGenerator::global());

    m_apiBases             = std::move(apiBases);
    m_apiIndex             = 0;
    m_apiDiscoveryFinished = true;

    auto pendingStationRequest   = std::exchange(m_pendingStationRequest, {});
    auto pendingLookupRequest    = std::exchange(m_pendingLookupRequest, {});
    auto pendingClickRequest     = std::exchange(m_pendingClickRequest, {});
    auto pendingCategoryRequests = std::move(m_pendingCategoryRequests);

    m_pendingCategoryRequests.clear();

    if(m_apiLookup) {
        m_apiLookup->deleteLater();
        m_apiLookup = nullptr;
    }

    if(pendingStationRequest) {
        PendingApiRequest pending;
        pending.type    = PendingRequestType::Station;
        pending.station = *pendingStationRequest;
        startPendingRequest(pending);
    }

    if(pendingLookupRequest) {
        PendingApiRequest pending;
        pending.type   = PendingRequestType::Lookup;
        pending.lookup = *pendingLookupRequest;
        startPendingRequest(pending);
    }

    for(const auto& category : pendingCategoryRequests | std::views::values) {
        PendingApiRequest pending;
        pending.type     = PendingRequestType::Category;
        pending.category = category;
        startPendingRequest(pending);
    }

    if(pendingClickRequest) {
        PendingApiRequest pending;
        pending.type        = PendingRequestType::Click;
        pending.stationUuid = *pendingClickRequest;
        startPendingRequest(pending);
    }
}

void RadioBrowserService::handleReply()
{
    if(!m_reply) {
        Q_EMIT searchFailed(stationRequestText(), tr("Request was cancelled."));
        return;
    }

    if(m_reply->error() != QNetworkReply::NoError) {
        const QString error = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        if(!retryStationRequest()) {
            const QString requestText = stationRequestText();
            m_stationRequest.reset();
            Q_EMIT searchFailed(requestText, error);
        }
        return;
    }

    const QByteArray data = readReplyData(m_reply);

    if(m_stationRequest) {
        m_apiIndex = m_stationRequest->apiIndex;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if(parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        const QString requestText = stationRequestText();
        m_stationRequest.reset();
        m_reply->deleteLater();
        m_reply = nullptr;
        Q_EMIT searchFailed(requestText, tr("Failed to parse station data."));
        return;
    }

    RadioStationList stations = stationsFromJsonArray(doc.array());

    m_reply->deleteLater();
    m_reply = nullptr;

    const QString requestText = stationRequestText();
    m_stationRequest.reset();
    Q_EMIT searchFinished(requestText, stations);
}

void RadioBrowserService::handleStationLookupReply()
{
    if(!m_lookupReply || !m_lookupRequest) {
        return;
    }

    if(m_lookupReply->error() != QNetworkReply::NoError) {
        const QString error = m_lookupReply->errorString();
        m_lookupReply->deleteLater();
        m_lookupReply = nullptr;
        if(!retryStationLookupRequest()) {
            const int requestId = m_lookupRequest ? m_lookupRequest->requestId : 0;
            m_lookupRequest.reset();
            Q_EMIT stationLookupFailed(requestId, error);
        }
        return;
    }

    const QByteArray data = readReplyData(m_lookupReply);
    m_apiIndex            = m_lookupRequest->apiIndex;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if(parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        const int requestId = m_lookupRequest->requestId;
        m_lookupRequest.reset();
        m_lookupReply->deleteLater();
        m_lookupReply = nullptr;
        Q_EMIT stationLookupFailed(requestId, tr("Failed to parse station data."));
        return;
    }

    const int requestId        = m_lookupRequest->requestId;
    RadioStationList stations  = stationsFromJsonArray(doc.array());
    m_lookupRequest            = {};
    QNetworkReply* lookupReply = m_lookupReply;
    m_lookupReply              = nullptr;
    lookupReply->deleteLater();
    Q_EMIT stationLookupFinished(requestId, stations);
}

void RadioBrowserService::fetchStationsEndpoint(const QString& path, const QString& label, int limit, int offset)
{
    if(m_reply) {
        QObject::disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
    }

    Q_EMIT searchStarted(label);

    PendingApiRequest pending;
    pending.type                 = PendingRequestType::Station;
    pending.station.type         = StationRequestType::Endpoint;
    pending.station.endpointPath = path;
    pending.station.limit        = limit;
    pending.station.offset       = offset;
    pending.station.apiIndex     = m_apiIndex;
    queueOrStart(pending);
}

void RadioBrowserService::handleCategoriesReply(const RadioCategoryType type)
{
    const auto replyIt                  = m_categoryReplies.find(type);
    const QPointer<QNetworkReply> reply = replyIt != m_categoryReplies.end() ? replyIt->second : nullptr;
    if(!reply) {
        Q_EMIT categoriesFailed(type, tr("Request was cancelled."));
        return;
    }

    if(reply->error() != QNetworkReply::NoError) {
        const QString error = reply->errorString();
        reply->deleteLater();
        m_categoryReplies.erase(type);

        if(!retryCategoryRequest(type)) {
            m_categoryRequests.erase(type);
            Q_EMIT categoriesFailed(type, error);
        }
        return;
    }

    const QByteArray data = readReplyData(reply);

    const auto contextIt = m_categoryRequests.find(type);
    if(contextIt != m_categoryRequests.end()) {
        m_apiIndex = contextIt->second.apiIndex;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if(parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        m_categoryRequests.erase(type);
        reply->deleteLater();
        m_categoryReplies.erase(type);
        Q_EMIT categoriesFailed(type, tr("Failed to parse category data."));
        return;
    }

    const QJsonArray array = doc.array();
    RadioCategoryList categories;
    categories.reserve(array.size());

    for(const auto& value : array) {
        if(!value.isObject()) {
            continue;
        }

        RadioCategory category = categoryFromObject(type, value.toObject());
        if(!category.name.isEmpty() && !category.value.isEmpty()) {
            categories.push_back(std::move(category));
        }
    }

    reply->deleteLater();
    m_categoryReplies.erase(type);
    m_categoryRequests.erase(type);
    Q_EMIT categoriesFinished(type, categories);
}

QString RadioBrowserService::stationRequestText() const
{
    if(!m_stationRequest) {
        return {};
    }

    switch(m_stationRequest->type) {
        case StationRequestType::Search:
            return m_stationRequest->searchRequest.text.trimmed();
        case StationRequestType::Endpoint:
            return m_stationRequest->endpointPath;
        case StationRequestType::None:
            break;
    }

    return {};
}

QString RadioBrowserService::apiBase(int index) const
{
    if(m_apiBases.empty()) {
        return QString::fromLatin1(FallbackApiBases.front());
    }

    return m_apiBases.at(std::clamp<qsizetype>(index, 0, m_apiBases.size() - 1));
}

bool RadioBrowserService::retryStationRequest()
{
    if(!m_stationRequest) {
        return false;
    }

    if(m_stationRequest->attempts >= m_apiBases.size()) {
        return false;
    }

    const StationRequestContext request = *m_stationRequest;
    const int nextIndex                 = (request.apiIndex + 1) % static_cast<int>(m_apiBases.size());
    const int nextAttempts              = request.attempts + 1;

    if(request.type == StationRequestType::Search) {
        startSearchRequest(request.searchRequest, nextIndex, nextAttempts);
    }
    else if(request.type == StationRequestType::Endpoint) {
        startEndpointRequest(request.endpointPath, request.limit, request.offset, nextIndex, nextAttempts);
    }
    else {
        return false;
    }

    return true;
}

bool RadioBrowserService::retryStationLookupRequest()
{
    if(!m_lookupRequest) {
        return false;
    }

    if(m_lookupRequest->attempts >= m_apiBases.size()) {
        return false;
    }

    StationLookupRequestContext request = *m_lookupRequest;
    request.apiIndex                    = (request.apiIndex + 1) % static_cast<int>(m_apiBases.size());
    ++request.attempts;
    startStationLookupRequest(request);
    return true;
}

bool RadioBrowserService::retryCategoryRequest(RadioCategoryType type)
{
    const auto requestIt = m_categoryRequests.find(type);
    if(requestIt == m_categoryRequests.end()) {
        return false;
    }

    if(requestIt->second.attempts >= m_apiBases.size()) {
        return false;
    }

    const CategoryRequestContext request = requestIt->second;
    const int nextIndex                  = (request.apiIndex + 1) % static_cast<int>(m_apiBases.size());
    startCategoryRequest(request.type, nextIndex, request.attempts + 1);
    return true;
}

QUrl RadioBrowserService::buildSearchUrl(const RadioSearchRequest& request, int apiIndex) const
{
    QUrl url{apiBase(apiIndex) + "/json/stations/search"_L1};

    QUrlQuery query;

    if(const QString nameText = request.text.trimmed(); !nameText.isEmpty()) {
        query.addQueryItem(u"name"_s, nameText);
    }
    if(!request.countryCode.trimmed().isEmpty()) {
        query.addQueryItem(u"countrycode"_s, request.countryCode.trimmed());
    }
    if(!request.language.trimmed().isEmpty()) {
        query.addQueryItem(u"language"_s, request.language.trimmed());
    }

    const QStringList tags = splitStationTags(request.tag);
    if(tags.size() > 1) {
        query.addQueryItem(u"tagList"_s, tags.join(u","_s));
    }
    else if(tags.size() == 1) {
        query.addQueryItem(u"tag"_s, tags.front());
    }

    if(request.bitrateMin > 0) {
        query.addQueryItem(u"bitrateMin"_s, QString::number(request.bitrateMin));
    }
    if(request.bitrateMax > 0) {
        query.addQueryItem(u"bitrateMax"_s, QString::number(request.bitrateMax));
    }

    query.addQueryItem(u"hidebroken"_s, request.hideBroken ? u"true"_s : u"false"_s);
    query.addQueryItem(u"limit"_s, QString::number(request.limit > 0 ? request.limit : Limit));
    query.addQueryItem(u"offset"_s, QString::number(std::max(0, request.offset)));
    query.addQueryItem(u"reverse"_s, request.reverse ? u"true"_s : u"false"_s);
    query.addQueryItem(u"order"_s, request.order.isEmpty() ? u"clickcount"_s : request.order);

    QString queryString = query.toString(QUrl::FullyEncoded);
    appendEncodedQueryItem(queryString, u"codec"_s, request.codec.trimmed());
    url.setQuery(queryString, QUrl::StrictMode);

    return url;
}

QUrl RadioBrowserService::buildStationsEndpointUrl(const QString& path, int limit, int offset, int apiIndex) const
{
    QUrl url{apiBase(apiIndex) + u"/json/%1/%2"_s.arg(path, QString::number(limit > 0 ? limit : Limit))};

    QUrlQuery query;
    query.addQueryItem(u"offset"_s, QString::number(std::max(0, offset)));
    query.addQueryItem(u"hidebroken"_s, u"true"_s);
    url.setQuery(query);

    return url;
}

QUrl RadioBrowserService::buildStationLookupUrl(const QStringList& uuids, int apiIndex) const
{
    QUrl url{apiBase(apiIndex) + "/json/stations/byuuid"_L1};

    QUrlQuery query;
    query.addQueryItem(u"uuids"_s, uuids.join(u','));
    query.addQueryItem(u"hidebroken"_s, u"true"_s);
    url.setQuery(query);

    return url;
}

QUrl RadioBrowserService::buildStationUrlLookupUrl(const QString& stationUrl, int apiIndex) const
{
    QUrl url{apiBase(apiIndex) + "/json/stations/byurl"_L1};

    QUrlQuery query;
    query.addQueryItem(u"url"_s, stationUrl);
    query.addQueryItem(u"hidebroken"_s, u"true"_s);
    url.setQuery(query);

    return url;
}

QUrl RadioBrowserService::buildCategoryUrl(const RadioCategoryType type, const int apiIndex) const
{
    QUrl url{apiBase(apiIndex) + u"/json/%1"_s.arg(categoryPath(type))};

    if(type == RadioCategoryType::Tag) {
        QUrlQuery query;
        query.addQueryItem(u"limit"_s, u"20000"_s);
        url.setQuery(query);
    }

    return url;
}

QUrl RadioBrowserService::buildClickUrl(const QString& stationUuid) const
{
    return QUrl{apiBase(m_apiIndex) + u"/json/url/%1"_s.arg(stationUuid)};
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiobrowserservice.cpp"
