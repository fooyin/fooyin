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

#pragma once

#include "radiodiscovery.h"
#include "radiostation.h"
#include "radiostationm3u.h"
#include "radiostationstore.h"

#include <core/track.h>

#include <QObject>

#include <optional>
#include <unordered_map>
#include <vector>

namespace Fooyin {
class NetworkAccessManager;
class PlayerController;
class PlaylistLoader;
class SettingsManager;

namespace RadioBrowser {
class RadioBrowserService;
class RadioIconProvider;
class RadioStreamResolver;

class RadioBrowserController : public QObject
{
    Q_OBJECT

public:
    explicit RadioBrowserController(std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings,
                                    PlayerController* playerController, std::shared_ptr<PlaylistLoader> playlistLoader,
                                    RadioStationStore* store, bool reportPlaybackStats = true,
                                    QObject* parent = nullptr);

    [[nodiscard]] RadioStationList stations() const;
    [[nodiscard]] RadioStationList savedStations() const;
    [[nodiscard]] RadioSavedSearchList savedSearches() const;
    [[nodiscard]] RadioIconProvider* iconProvider() const;

    [[nodiscard]] bool isSaved(const RadioStation& station) const;

    [[nodiscard]] bool hideBroken() const;
    void setHideBroken(bool hideBroken);

    [[nodiscard]] bool stationRequestActive() const;
    [[nodiscard]] bool browsingSavedStations() const;
    [[nodiscard]] bool hasActivatedBrowse() const;
    void clearActivatedBrowse();

    [[nodiscard]] RadioSearchRequest currentRequest() const;
    [[nodiscard]] std::optional<RadioSearchRequest> latestSearchRequest() const;
    [[nodiscard]] std::optional<RadioSavedSearch> savedSearchForRequest(const RadioSearchRequest& request) const;

    void searchStations(const QString& query);
    void searchStations(const RadioSearchRequest& request);
    void manualSearchStations(const RadioSearchRequest& request);
    void activateSavedSearch(const RadioSavedSearch& search);
    void sortCurrentStations(const QString& order, bool reverse);

    void fetchTopClicked();
    void fetchTopVoted();
    void fetchRecentlyClicked();
    void fetchRecentlyChanged();
    void fetchNowListening();
    void fetchRandom();
    void fetchCategories(RadioCategoryType type);

    void browseCategory(const RadioCategory& category);
    void browseSavedStations();

    [[nodiscard]] bool canLoadMoreStations() const;
    void loadMoreStations();
    [[nodiscard]] int resolveStations(const RadioStationList& stations, QObject* context = nullptr);
    static Track trackForStation(const RadioStation& station, const QString& streamUrl = {});

    void saveStation(const RadioStation& station);
    void addLocalStation(const RadioStation& station);
    void updateSavedStation(const RadioStation& previousStation, const RadioStation& station);
    void removeSavedStation(const RadioStation& station);
    void reorderSavedStations(const RadioStationList& stations);

    [[nodiscard]] bool exportSavedStationsToM3u(const QString& filePath, QString* error = nullptr) const;
    void importSavedStationsFromM3u(const QString& filePath, SavedStationImportMode mode, QObject* context = nullptr);

    void saveSearch(const QString& name, const RadioSearchRequest& request);
    void renameSearch(const QString& id, const QString& name);
    void removeSearch(const QString& id);

    void setSendClicks(bool enabled);
    void reportStationClick(const RadioStation& station);
    void validateStation(const RadioStation& station, QObject* context = nullptr);
    void lookupStationByUrl(const QString& url, QObject* context = nullptr);

Q_SIGNALS:
    void searchStarted(const QString& query);
    void searchFailed(const QString& query, const QString& error);

    void stationsChanged(const Fooyin::RadioBrowser::RadioStationList& stations, bool reset);
    void stationActionFailed(QObject* context, const Fooyin::RadioBrowser::RadioStation& station, const QString& error);
    void stationsResolved(int resolveId, QObject* context, const Fooyin::TrackList& tracks);
    void stationValidated(QObject* context, const Fooyin::RadioBrowser::RadioStation& station);
    void stationValidationFailed(QObject* context, const QString& error);
    void stationUrlLookupFinished(QObject* context, const Fooyin::RadioBrowser::RadioStationList& stations);
    void stationUrlLookupFailed(QObject* context, const QString& error);

    void savedStationsBrowsingChanged(bool browsing);
    void savedStationsChanged(const Fooyin::RadioBrowser::RadioStationList& stations);
    void savedStationsImportFinished(QObject* context, Fooyin::RadioBrowser::SavedStationImportResult result);
    void savedStationsImportFailed(QObject* context, const QString& error);
    void savedSearchesChanged(const Fooyin::RadioBrowser::RadioSavedSearchList& searches);

    void latestSearchChanged(const Fooyin::RadioBrowser::RadioSearchRequest& request);
    void searchRequestActivated(const Fooyin::RadioBrowser::RadioSearchRequest& request);
    void currentStationChanged(const Fooyin::RadioBrowser::RadioStation& station);

    void categoriesStarted(Fooyin::RadioBrowser::RadioCategoryType type);
    void categoriesChanged(Fooyin::RadioBrowser::RadioCategoryType type,
                           const Fooyin::RadioBrowser::RadioCategoryList& categories);
    void categoriesFailed(Fooyin::RadioBrowser::RadioCategoryType type, const QString& error);

private:
    enum class StationSource
    {
        SearchResults,
        SavedStations
    };

    enum class StationRequestMode
    {
        Replace,
        Append
    };

    struct PendingResolve
    {
        int groupId{0};
        size_t index{0};
        QObject* context{nullptr};
        RadioStation station;
    };

    struct PendingResolveGroup
    {
        QObject* context{nullptr};
        int pending{0};
        std::vector<std::optional<Track>> tracks;
    };

    struct PendingStationImport
    {
        QObject* context{nullptr};
        std::vector<RadioStationM3uEntry> entries;
        SavedStationImportMode mode;
    };

    struct PendingStationUrlLookup
    {
        QObject* context{nullptr};
    };

    void loadLatestSearchState();
    void storeLatestSearchState() const;
    void setStationSource(StationSource source);
    void searchStations(const RadioSearchRequest& request, bool updateLatestSearch);

    void handleSearchFailed(const QString& query, const QString& error);
    void handleSearchFinished(const QString& query, const RadioStationList& stations);
    void handleCategoriesFailed(RadioCategoryType type, const QString& error);
    void handleSavedStationsChanged(const RadioStationList& stations, SavedStationChange change);
    void handleStationLookupFinished(int requestId, const RadioStationList& stations);
    void handleStationLookupFailed(int requestId, const QString& error);
    void handleResolvedStream(int requestId, const QString& originalUrl, const QStringList& streamUrls);
    void handleResolveFailed(int requestId, const QString& originalUrl, const QString& error);
    void handleCurrentTrackChanged(const Track& track);
    void finishResolveGroup(int groupId);

    RadioBrowserService* m_service;
    RadioIconProvider* m_iconProvider;
    RadioStationStore* m_store;
    RadioStreamResolver* m_resolver;
    PlayerController* m_playerController;

    RadioStationList m_stations;
    RadioSearchRequest m_currentRequest;
    std::optional<RadioSearchRequest> m_latestSearchRequest;
    StationSource m_stationSource;
    StationRequestMode m_stationRequestMode;
    std::unordered_map<int, PendingResolve> m_pendingResolves;
    std::unordered_map<int, PendingResolve> m_pendingValidations;
    std::unordered_map<int, PendingResolveGroup> m_pendingResolveGroups;
    std::unordered_map<int, PendingStationImport> m_pendingImports;
    std::unordered_map<int, PendingStationUrlLookup> m_pendingUrlLookups;
    int m_nextResolveId;
    int m_nextResolveGroupId;
    int m_nextImportRequestId;
    bool m_stationRequestActive;
    bool m_hasActivatedBrowse;
    bool m_canLoadMoreStations;
    bool m_currentRequestUpdatesLatestSearch;
    bool m_hideBroken;
    bool m_sendClicks;
};
} // namespace RadioBrowser
} // namespace Fooyin
