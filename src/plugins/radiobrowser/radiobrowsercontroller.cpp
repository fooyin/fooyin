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

#include "radiobrowsercontroller.h"

#include "radiobrowserservice.h"
#include "radiodiscovery.h"
#include "radioiconprovider.h"
#include "radiostationstore.h"
#include "radiostreamresolver.h"

#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <gui/statusevent.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <utility>

using namespace Qt::StringLiterals;

constexpr int StationPageSize    = 100;
constexpr auto LatestSearchState = "RadioBrowser/LatestSearch"_L1;

namespace Fooyin::RadioBrowser {
namespace {
template <typename Map>
std::optional<typename Map::mapped_type> takeValue(Map& map, const typename Map::key_type& key)
{
    const auto it = map.find(key);
    if(it == map.end()) {
        return {};
    }

    auto value = std::move(it->second);
    map.erase(it);
    return value;
}

QString propertyValue(const Track::ExtraProperties& properties, const QString& key)
{
    if(const QString value = properties.value(u"_%1"_s.arg(key)); !value.isEmpty()) {
        return value;
    }
    return properties.value(key);
}

QString categoryTypeName(const RadioCategoryType type)
{
    switch(type) {
        case RadioCategoryType::Country:
            return RadioBrowserController::tr("countries");
        case RadioCategoryType::Language:
            return RadioBrowserController::tr("languages");
        case RadioCategoryType::Tag:
            return RadioBrowserController::tr("tags");
        case RadioCategoryType::Codec:
            return RadioBrowserController::tr("codecs");
    }

    return RadioBrowserController::tr("categories");
}

Track applyStationMetadata(Track track, const RadioStation& station)
{
    if(!station.name.isEmpty()) {
        track.setTitle(station.name);
    }
    if(!station.tags.isEmpty()) {
        track.setGenres(splitStationTags(station.tags));
    }
    if(!station.codec.isEmpty()) {
        track.setCodec(station.codec);
    }
    if(station.bitrate > 0) {
        track.setBitrate(station.bitrate);
    }
    if(!station.favicon.isEmpty()) {
        track.replaceExtraTag(u"COVERART"_s, station.favicon);
    }

    track.setMetadataWasRead(true);
    track.setExtraProperty(u"_STREAM_PROVIDER"_s, station.local ? u"local"_s : u"radio-browser.info"_s);
    track.setExtraProperty(u"_RADIO_BROWSER_STATIONUUID"_s, station.uuid);
    track.setExtraProperty(u"_RADIO_BROWSER_STATIONNAME"_s, station.name);
    track.setExtraProperty(u"_RADIO_BROWSER_STREAMURL"_s, station.streamUrl);
    track.setExtraProperty(u"_RADIO_BROWSER_RESOLVED_STREAMURL"_s, track.filepath());
    track.setExtraProperty(u"_RADIO_BROWSER_HOMEPAGE"_s, station.homepage);
    track.setExtraProperty(u"_RADIO_BROWSER_FAVICON"_s, station.favicon);
    track.setExtraProperty(u"_RADIO_BROWSER_COUNTRY"_s, station.country);
    track.setExtraProperty(u"_RADIO_BROWSER_COUNTRYCODE"_s, station.countryCode);
    track.setExtraProperty(u"_RADIO_BROWSER_LANGUAGE"_s, station.language);
    track.setExtraProperty(u"_RADIO_BROWSER_HLS"_s, station.hls ? u"true"_s : u"false"_s);

    return track;
}

RadioStation stationFromTrack(const Track& track)
{
    const Track::ExtraProperties properties = track.extraProperties();

    RadioStation station;
    station.uuid              = propertyValue(properties, u"RADIO_BROWSER_STATIONUUID"_s);
    station.name              = propertyValue(properties, u"RADIO_BROWSER_STATIONNAME"_s);
    station.streamUrl         = propertyValue(properties, u"RADIO_BROWSER_STREAMURL"_s);
    station.resolvedStreamUrl = propertyValue(properties, u"RADIO_BROWSER_RESOLVED_STREAMURL"_s);
    station.homepage          = propertyValue(properties, u"RADIO_BROWSER_HOMEPAGE"_s);
    station.favicon           = propertyValue(properties, u"RADIO_BROWSER_FAVICON"_s);
    station.country           = propertyValue(properties, u"RADIO_BROWSER_COUNTRY"_s);
    station.countryCode       = propertyValue(properties, u"RADIO_BROWSER_COUNTRYCODE"_s);
    station.language          = propertyValue(properties, u"RADIO_BROWSER_LANGUAGE"_s);
    station.hls               = propertyValue(properties, u"RADIO_BROWSER_HLS"_s) == u"true"_s;

    const QString provider = propertyValue(properties, u"STREAM_PROVIDER"_s);
    station.local          = provider == u"local"_s;

    if(provider != u"radio-browser.info"_s && !station.local) {
        return {};
    }

    if(station.name.isEmpty()) {
        station.name = track.effectiveTitle();
    }
    station.tags    = track.genres().join(u", "_s);
    station.codec   = track.codec();
    station.bitrate = track.bitrate();
    if(station.streamUrl.isEmpty()) {
        station.streamUrl = track.filepath();
    }
    if(station.resolvedStreamUrl.isEmpty()) {
        station.resolvedStreamUrl = track.filepath();
    }

    return station;
}

RadioStation fallbackStationForEntry(const RadioStationM3uEntry& entry)
{
    RadioStation station;
    station.uuid      = entry.uuid;
    station.name      = entry.name.trimmed().isEmpty() ? entry.streamUrl : entry.name.trimmed();
    station.streamUrl = entry.streamUrl.trimmed();
    station.local     = entry.uuid.trimmed().isEmpty();
    return station;
}

const RadioStation* findStationByUuid(const RadioStationList& stations, const QString& uuid)
{
    if(uuid.trimmed().isEmpty()) {
        return nullptr;
    }

    for(const RadioStation& station : stations) {
        if(station.uuid == uuid) {
            return &station;
        }
    }

    return nullptr;
}
} // namespace

RadioBrowserController::RadioBrowserController(std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings,
                                               PlayerController* playerController,
                                               std::shared_ptr<PlaylistLoader> playlistLoader, RadioStationStore* store,
                                               const bool reportPlaybackStats, QObject* parent)
    : QObject{parent}
    , m_service{new RadioBrowserService(network, settings, this)}
    , m_iconProvider{new RadioIconProvider(network, this)}
    , m_store{store}
    , m_resolver{new RadioStreamResolver(std::move(network), std::move(playlistLoader), this)}
    , m_playerController{playerController}
    , m_stationSource{StationSource::SearchResults}
    , m_stationRequestMode{StationRequestMode::Replace}
    , m_nextResolveId{0}
    , m_nextResolveGroupId{0}
    , m_nextImportRequestId{0}
    , m_stationRequestActive{false}
    , m_hasActivatedBrowse{false}
    , m_canLoadMoreStations{false}
    , m_currentRequestUpdatesLatestSearch{false}
    , m_hideBroken{true}
    , m_sendClicks{true}
{
    QObject::connect(m_service, &RadioBrowserService::searchStarted, this, &RadioBrowserController::searchStarted);
    QObject::connect(m_service, &RadioBrowserService::searchFailed, this, &RadioBrowserController::handleSearchFailed);
    QObject::connect(m_service, &RadioBrowserService::searchFinished, this,
                     &RadioBrowserController::handleSearchFinished);
    QObject::connect(m_service, &RadioBrowserService::categoriesStarted, this,
                     &RadioBrowserController::categoriesStarted);
    QObject::connect(m_service, &RadioBrowserService::categoriesFinished, this,
                     &RadioBrowserController::categoriesChanged);
    QObject::connect(m_service, &RadioBrowserService::categoriesFailed, this,
                     &RadioBrowserController::handleCategoriesFailed);
    QObject::connect(m_service, &RadioBrowserService::stationLookupFinished, this,
                     &RadioBrowserController::handleStationLookupFinished);
    QObject::connect(m_service, &RadioBrowserService::stationLookupFailed, this,
                     &RadioBrowserController::handleStationLookupFailed);

    QObject::connect(m_store, &RadioStationStore::savedSearchesChanged, this,
                     &RadioBrowserController::savedSearchesChanged);
    QObject::connect(m_store, &RadioStationStore::savedStationsChanged, this,
                     &RadioBrowserController::handleSavedStationsChanged);

    QObject::connect(m_resolver, &RadioStreamResolver::resolved, this, &RadioBrowserController::handleResolvedStream);
    QObject::connect(m_resolver, &RadioStreamResolver::failed, this, &RadioBrowserController::handleResolveFailed);

    if(reportPlaybackStats) {
        QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                         &RadioBrowserController::handleCurrentTrackChanged);
    }

    loadLatestSearchState();
}

RadioStationList RadioBrowserController::stations() const
{
    return m_stations;
}

RadioStationList RadioBrowserController::savedStations() const
{
    return m_store->savedStations();
}

RadioSavedSearchList RadioBrowserController::savedSearches() const
{
    return m_store->savedSearches();
}

RadioIconProvider* RadioBrowserController::iconProvider() const
{
    return m_iconProvider;
}

bool RadioBrowserController::isSaved(const RadioStation& station) const
{
    return m_store->isSaved(station);
}

bool RadioBrowserController::hideBroken() const
{
    return m_hideBroken;
}

void RadioBrowserController::setHideBroken(const bool hideBroken)
{
    m_hideBroken = hideBroken;
}

bool RadioBrowserController::stationRequestActive() const
{
    return m_stationRequestActive;
}

bool RadioBrowserController::browsingSavedStations() const
{
    return m_stationSource == StationSource::SavedStations;
}

bool RadioBrowserController::hasActivatedBrowse() const
{
    return m_hasActivatedBrowse;
}

std::optional<RadioSearchRequest> RadioBrowserController::latestSearchRequest() const
{
    return m_latestSearchRequest;
}

std::optional<RadioSavedSearch> RadioBrowserController::savedSearchForRequest(const RadioSearchRequest& request) const
{
    return m_store->savedSearchForRequest(request);
}

void RadioBrowserController::searchStations(const QString& query)
{
    RadioSearchRequest request;
    request.text       = query;
    request.order      = u"clickcount"_s;
    request.hideBroken = m_hideBroken;
    searchStations(request);
}

void RadioBrowserController::searchStations(const RadioSearchRequest& request)
{
    searchStations(request, false);
}

void RadioBrowserController::manualSearchStations(const RadioSearchRequest& request)
{
    searchStations(request, true);
}

void RadioBrowserController::activateSavedSearch(const RadioSavedSearch& search)
{
    searchStations(search.request);
}

void RadioBrowserController::sortCurrentStations(const QString& order, const bool reverse)
{
    if(m_stationSource == StationSource::SavedStations || m_currentRequest.order == "random"_L1) {
        return;
    }

    RadioSearchRequest request{m_currentRequest};
    request.order   = order;
    request.reverse = reverse;
    request.offset  = 0;

    searchStations(request, m_currentRequestUpdatesLatestSearch);
}

void RadioBrowserController::fetchTopClicked()
{
    RadioSearchRequest request;
    request.order      = u"clickcount"_s;
    request.hideBroken = m_hideBroken;
    searchStations(request);
}

void RadioBrowserController::fetchTopVoted()
{
    RadioSearchRequest request;
    request.order      = u"votes"_s;
    request.reverse    = true;
    request.hideBroken = m_hideBroken;
    searchStations(request);
}

void RadioBrowserController::fetchRecentlyClicked()
{
    RadioSearchRequest request;
    request.order      = u"clicktrend"_s;
    request.reverse    = true;
    request.hideBroken = m_hideBroken;
    searchStations(request);
}

void RadioBrowserController::fetchRecentlyChanged()
{
    RadioSearchRequest request;
    request.order      = u"changetimestamp"_s;
    request.reverse    = true;
    request.hideBroken = m_hideBroken;
    searchStations(request);
}

void RadioBrowserController::fetchNowListening()
{
    RadioSearchRequest request;
    request.order      = u"clicktimestamp"_s;
    request.reverse    = true;
    request.hideBroken = m_hideBroken;
    searchStations(request);
}

void RadioBrowserController::fetchRandom()
{
    RadioSearchRequest request;
    request.order      = u"random"_s;
    request.reverse    = false;
    request.hideBroken = m_hideBroken;
    searchStations(request);
}

void RadioBrowserController::fetchCategories(const RadioCategoryType type)
{
    m_service->fetchCategories(type);
}

void RadioBrowserController::browseCategory(const RadioCategory& category)
{
    RadioSearchRequest request;
    request.limit      = 100;
    request.order      = u"clickcount"_s;
    request.hideBroken = m_hideBroken;

    switch(category.type) {
        case RadioCategoryType::Country:
            request.countryCode = category.value;
            break;
        case RadioCategoryType::Language:
            request.language = category.value;
            break;
        case RadioCategoryType::Tag:
            request.tag = category.value;
            break;
        case RadioCategoryType::Codec:
            request.codec = category.value;
            break;
    }

    searchStations(request);
}

void RadioBrowserController::browseSavedStations()
{
    m_hasActivatedBrowse = true;
    setStationSource(StationSource::SavedStations);
    m_stations                          = m_store->savedStations();
    m_stationRequestActive              = false;
    m_stationRequestMode                = StationRequestMode::Replace;
    m_canLoadMoreStations               = false;
    m_currentRequestUpdatesLatestSearch = false;
    Q_EMIT stationsChanged(m_stations, true);
}

bool RadioBrowserController::canLoadMoreStations() const
{
    return m_canLoadMoreStations && !m_stationRequestActive;
}

void RadioBrowserController::loadMoreStations()
{
    if(!canLoadMoreStations()) {
        return;
    }

    RadioSearchRequest request{m_currentRequest};
    request.offset         = static_cast<int>(m_stations.size());
    request.limit          = StationPageSize;
    m_currentRequest       = request;
    m_stationRequestActive = true;
    m_stationRequestMode   = StationRequestMode::Append;
    m_canLoadMoreStations  = false;

    m_service->searchStations(m_currentRequest);
}

int RadioBrowserController::resolveStations(const RadioStationList& stations, QObject* context)
{
    const int groupId = ++m_nextResolveGroupId;

    PendingResolveGroup group;
    group.context = context;
    group.tracks.resize(stations.size());

    struct ResolveRequest
    {
        int id;
        QString streamUrl;
    };
    std::vector<ResolveRequest> requests;
    requests.reserve(stations.size());

    for(size_t trackIndex{0}; const RadioStation& station : stations) {
        const QString streamUrl = station.effectiveStreamUrl();
        if(streamUrl.isEmpty()) {
            Q_EMIT stationActionFailed(context, station, tr("Station has no stream URL."));
            continue;
        }

        const int requestId = ++m_nextResolveId;
        m_pendingResolves.emplace(
            requestId,
            PendingResolve{.groupId = groupId, .index = trackIndex++, .context = context, .station = station});
        requests.emplace_back(ResolveRequest{.id = requestId, .streamUrl = streamUrl});
    }

    if(requests.empty()) {
        return 0;
    }

    group.pending = static_cast<int>(requests.size());
    m_pendingResolveGroups.emplace(groupId, std::move(group));

    for(const ResolveRequest& request : requests) {
        m_resolver->resolve(request.id, request.streamUrl);
    }

    return groupId;
}

Track RadioBrowserController::trackForStation(const RadioStation& station, const QString& streamUrl)
{
    Track track{streamUrl.isEmpty() ? station.effectiveStreamUrl() : streamUrl};
    track.setTitle(station.name);
    return applyStationMetadata(track, station);
}

void RadioBrowserController::saveStation(const RadioStation& station)
{
    m_store->saveStation(station);
}

void RadioBrowserController::addLocalStation(const RadioStation& station)
{
    RadioStation localStation{station};
    localStation.local = true;
    m_store->saveStation(localStation);
}

void RadioBrowserController::updateSavedStation(const RadioStation& previousStation, const RadioStation& station)
{
    RadioStation updatedStation{station};
    updatedStation.local = previousStation.local;
    m_store->updateStation(previousStation, updatedStation);
}

void RadioBrowserController::removeSavedStation(const RadioStation& station)
{
    m_store->removeStation(station);
}

void RadioBrowserController::reorderSavedStations(const RadioStationList& stations)
{
    m_store->reorderStations(stations);
}

bool RadioBrowserController::exportSavedStationsToM3u(const QString& filePath, QString* error) const
{
    return writeRadioStationM3u(filePath, m_store->savedStations(), error);
}

void RadioBrowserController::importSavedStationsFromM3u(const QString& filePath, const SavedStationImportMode mode,
                                                        QObject* context)
{
    if(!m_pendingImports.empty()) {
        Q_EMIT savedStationsImportFailed(context, tr("A radio station import is already in progress."));
        return;
    }

    RadioStationM3uReadResult readResult = readRadioStationM3u(filePath);
    if(!readResult.error.isEmpty()) {
        Q_EMIT savedStationsImportFailed(context, readResult.error);
        return;
    }

    QStringList uuids;
    for(const RadioStationM3uEntry& entry : readResult.entries) {
        if(!entry.uuid.trimmed().isEmpty() && !uuids.contains(entry.uuid.trimmed())) {
            uuids.push_back(entry.uuid.trimmed());
        }
    }

    if(uuids.empty()) {
        RadioStationList stations;
        stations.reserve(readResult.entries.size());
        for(const RadioStationM3uEntry& entry : readResult.entries) {
            stations.emplace_back(fallbackStationForEntry(entry));
        }
        Q_EMIT savedStationsImportFinished(context, m_store->importStations(stations, mode));
        return;
    }

    const int requestId = ++m_nextImportRequestId;
    m_pendingImports.emplace(
        requestId, PendingStationImport{.context = context, .entries = std::move(readResult.entries), .mode = mode});
    m_service->fetchStationsByUuids(requestId, uuids);
}

void RadioBrowserController::saveSearch(const QString& name, const RadioSearchRequest& request)
{
    m_store->saveSearch(name, request);
}

void RadioBrowserController::renameSearch(const QString& id, const QString& name)
{
    m_store->renameSearch(id, name);
}

void RadioBrowserController::removeSearch(const QString& id)
{
    m_store->removeSearch(id);
}

void RadioBrowserController::setSendClicks(const bool enabled)
{
    m_sendClicks = enabled;
}

void RadioBrowserController::reportStationClick(const RadioStation& station)
{
    if(m_sendClicks && !station.local) {
        m_service->reportClick(station.uuid);
    }
}

void RadioBrowserController::validateStation(const RadioStation& station, QObject* context)
{
    const QString streamUrl = station.effectiveStreamUrl();
    if(streamUrl.isEmpty()) {
        Q_EMIT stationValidationFailed(context, tr("Station has no stream URL."));
        return;
    }

    const int requestId = ++m_nextResolveId;
    m_pendingValidations.emplace(requestId, PendingResolve{.context = context, .station = station});
    m_resolver->resolve(requestId, streamUrl);
}

void RadioBrowserController::lookupStationByUrl(const QString& url, QObject* context)
{
    const QString stationUrl = url.trimmed();
    if(stationUrl.isEmpty()) {
        Q_EMIT stationUrlLookupFinished(context, {});
        return;
    }

    const int requestId = ++m_nextImportRequestId;
    m_pendingUrlLookups.emplace(requestId, PendingStationUrlLookup{.context = context});
    m_service->fetchStationsByUrl(requestId, stationUrl);
}

void RadioBrowserController::loadLatestSearchState()
{
    const FyStateSettings stateSettings;
    const QByteArray state = stateSettings.value(LatestSearchState).toByteArray();
    if(state.isEmpty()) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(state);
    if(!doc.isObject()) {
        return;
    }

    RadioSearchRequest request = searchRequestFromJson(doc.object());
    request.offset             = 0;
    request.limit              = 0;

    if(isDefaultSearchRequest(request)) {
        return;
    }

    m_latestSearchRequest = request;
}

void RadioBrowserController::storeLatestSearchState() const
{
    FyStateSettings stateSettings;
    if(!m_latestSearchRequest.has_value() || isDefaultSearchRequest(*m_latestSearchRequest)) {
        stateSettings.remove(LatestSearchState);
        return;
    }

    stateSettings.setValue(LatestSearchState,
                           QJsonDocument{searchRequestToJson(*m_latestSearchRequest)}.toJson(QJsonDocument::Compact));
}

void RadioBrowserController::setStationSource(const StationSource source)
{
    if(m_stationSource == source) {
        return;
    }

    m_stationSource = source;
    Q_EMIT savedStationsBrowsingChanged(m_stationSource == StationSource::SavedStations);
}

void RadioBrowserController::searchStations(const RadioSearchRequest& request, const bool updateLatestSearch)
{
    m_hasActivatedBrowse = true;
    setStationSource(StationSource::SearchResults);

    m_currentRequest                    = request;
    m_currentRequest.limit              = StationPageSize;
    m_currentRequest.offset             = 0;
    m_hideBroken                        = m_currentRequest.hideBroken;
    m_currentRequestUpdatesLatestSearch = updateLatestSearch;

    if(updateLatestSearch) {
        m_latestSearchRequest = m_currentRequest;
        storeLatestSearchState();
    }

    m_stationRequestActive = true;
    m_stationRequestMode   = StationRequestMode::Replace;
    m_canLoadMoreStations  = false;

    if(!m_stations.empty()) {
        m_stations.clear();
        Q_EMIT stationsChanged(m_stations, true);
    }
    if(updateLatestSearch) {
        Q_EMIT latestSearchChanged(m_currentRequest);
    }

    Q_EMIT searchRequestActivated(m_currentRequest);
    m_service->searchStations(m_currentRequest);
}

void RadioBrowserController::handleSearchFailed(const QString& query, const QString& error)
{
    const StationRequestMode requestMode{m_stationRequestMode};
    m_stationRequestActive = false;
    m_stationRequestMode   = StationRequestMode::Replace;
    m_canLoadMoreStations  = false;

    if(requestMode == StationRequestMode::Replace) {
        m_stations.clear();
        Q_EMIT stationsChanged(m_stations, true);
    }

    Q_EMIT searchFailed(query, error);
}

void RadioBrowserController::handleSearchFinished(const QString&, const RadioStationList& stations)
{
    const StationRequestMode requestMode{m_stationRequestMode};
    if(requestMode == StationRequestMode::Append) {
        m_stations.insert(m_stations.end(), stations.cbegin(), stations.cend());
    }
    else {
        m_stations = stations;
    }

    m_stationRequestActive = false;
    m_stationRequestMode   = StationRequestMode::Replace;
    m_canLoadMoreStations  = stations.size() >= static_cast<size_t>(m_currentRequest.limit);
    Q_EMIT stationsChanged(stations, requestMode == StationRequestMode::Replace);
}

void RadioBrowserController::handleCategoriesFailed(const RadioCategoryType type, const QString& error)
{
    if(!error.isEmpty()) {
        StatusEvent::post(tr("Failed to load radio browser %1: %2").arg(categoryTypeName(type), error));
    }
    Q_EMIT categoriesFailed(type, error);
}

void RadioBrowserController::handleSavedStationsChanged(const RadioStationList& stations,
                                                        const SavedStationChange change)
{
    Q_EMIT savedStationsChanged(stations);
    if(change == SavedStationChange::Order) {
        m_stations = stations;
        return;
    }

    if(m_stationSource == StationSource::SavedStations) {
        m_stations = stations;
        Q_EMIT stationsChanged(m_stations, true);
    }
}

void RadioBrowserController::handleStationLookupFinished(int requestId, const RadioStationList& stations)
{
    if(const auto pendingLookup = takeValue(m_pendingUrlLookups, requestId)) {
        Q_EMIT stationUrlLookupFinished(pendingLookup->context, stations);
        return;
    }

    const auto pending = takeValue(m_pendingImports, requestId);
    if(!pending) {
        return;
    }

    RadioStationList importedStations;
    importedStations.reserve(pending->entries.size());

    for(const RadioStationM3uEntry& entry : pending->entries) {
        if(const RadioStation* station = findStationByUuid(stations, entry.uuid)) {
            importedStations.emplace_back(*station);
        }
        else {
            importedStations.emplace_back(fallbackStationForEntry(entry));
        }
    }

    Q_EMIT savedStationsImportFinished(pending->context, m_store->importStations(importedStations, pending->mode));
}

void RadioBrowserController::handleStationLookupFailed(int requestId, const QString& error)
{
    if(const auto pendingLookup = takeValue(m_pendingUrlLookups, requestId)) {
        Q_EMIT stationUrlLookupFailed(pendingLookup->context, error);
        return;
    }

    const auto pending = takeValue(m_pendingImports, requestId);
    if(!pending) {
        return;
    }

    RadioStationList importedStations;
    importedStations.reserve(pending->entries.size());
    for(const RadioStationM3uEntry& entry : pending->entries) {
        importedStations.emplace_back(fallbackStationForEntry(entry));
    }

    StatusEvent::post(tr("Failed to refresh imported station metadata: %1").arg(error));
    Q_EMIT savedStationsImportFinished(pending->context, m_store->importStations(importedStations, pending->mode));
}

void RadioBrowserController::handleResolvedStream(int requestId, const QString& originalUrl,
                                                  const QStringList& streamUrls)
{
    if(const auto validation = takeValue(m_pendingValidations, requestId)) {
        RadioStation station{validation->station};

        if(station.streamUrl.isEmpty()) {
            station.streamUrl = originalUrl;
        }

        if(!streamUrls.empty()) {
            station.resolvedStreamUrl = streamUrls.front();
            Q_EMIT stationValidated(validation->context, station);
        }
        else {
            Q_EMIT stationValidationFailed(validation->context, tr("Station did not resolve to a playable stream."));
        }

        return;
    }

    const auto pending = takeValue(m_pendingResolves, requestId);
    if(!pending) {
        return;
    }

    if(streamUrls.empty()) {
        Q_EMIT stationActionFailed(pending->context, pending->station,
                                   tr("Station did not resolve to a playable stream."));
        finishResolveGroup(pending->groupId);
        return;
    }

    RadioStation station{pending->station};
    if(station.streamUrl.isEmpty()) {
        station.streamUrl = originalUrl;
    }
    station.resolvedStreamUrl = streamUrls.front();

    if(const auto groupIt = m_pendingResolveGroups.find(pending->groupId); groupIt != m_pendingResolveGroups.end()) {
        if(pending->index < groupIt->second.tracks.size()) {
            groupIt->second.tracks.at(pending->index) = trackForStation(station, streamUrls.front());
        }
    }

    finishResolveGroup(pending->groupId);
}

void RadioBrowserController::handleResolveFailed(const int requestId, const QString&, const QString& error)
{
    if(const auto validation = takeValue(m_pendingValidations, requestId)) {
        Q_EMIT stationValidationFailed(validation->context, error);
        return;
    }

    const auto pending = takeValue(m_pendingResolves, requestId);
    if(!pending) {
        return;
    }

    Q_EMIT stationActionFailed(pending->context, pending->station, error);
    finishResolveGroup(pending->groupId);
}

void RadioBrowserController::handleCurrentTrackChanged(const Track& track)
{
    const RadioStation station = stationFromTrack(track);
    if(station.effectiveStreamUrl().isEmpty()) {
        Q_EMIT currentStationChanged({});
        return;
    }

    m_store->recordPlayed(station);
    reportStationClick(station);
    Q_EMIT currentStationChanged(station);
}

void RadioBrowserController::finishResolveGroup(int groupId)
{
    const auto groupIt = m_pendingResolveGroups.find(groupId);
    if(groupIt == m_pendingResolveGroups.end()) {
        return;
    }

    --groupIt->second.pending;
    if(groupIt->second.pending > 0) {
        return;
    }

    TrackList tracks;
    tracks.reserve(groupIt->second.tracks.size());

    for(auto& track : groupIt->second.tracks) {
        if(track) {
            tracks.emplace_back(std::move(*track));
        }
    }

    QObject* context = groupIt->second.context;
    m_pendingResolveGroups.erase(groupIt);
    QMetaObject::invokeMethod(
        this,
        [this, groupId, context, tracks = std::move(tracks)]() { Q_EMIT stationsResolved(groupId, context, tracks); },
        Qt::QueuedConnection);
}
} // namespace Fooyin::RadioBrowser
