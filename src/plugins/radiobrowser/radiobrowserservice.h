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

#include <core/network/networkaccessmanager.h>

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>

#include <optional>
#include <unordered_map>

class QDnsLookup;

namespace Fooyin {
class SettingsManager;

namespace RadioBrowser {
class RadioBrowserService : public QObject
{
    Q_OBJECT

public:
    explicit RadioBrowserService(std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings,
                                 QObject* parent = nullptr);

    void searchStations(const QString& query);
    void searchStations(const RadioSearchRequest& request);

    void fetchTopClicked(int limit = 100, int offset = 0);
    void fetchTopVoted(int limit = 100, int offset = 0);
    void fetchRecentlyClicked(int limit = 100, int offset = 0);
    void fetchRecentlyChanged(int limit = 100, int offset = 0);
    void fetchCategories(RadioCategoryType type);
    void fetchStationsByUuids(int requestId, const QStringList& uuids);
    void fetchStationsByUrl(int requestId, const QString& url);

    void reportClick(const QString& stationUuid);

Q_SIGNALS:
    void searchStarted(const QString& query);
    void searchFinished(const QString& query, const Fooyin::RadioBrowser::RadioStationList& stations);
    void searchFailed(const QString& query, const QString& error);
    void categoriesStarted(Fooyin::RadioBrowser::RadioCategoryType type);
    void categoriesFinished(Fooyin::RadioBrowser::RadioCategoryType type,
                            const Fooyin::RadioBrowser::RadioCategoryList& categories);
    void categoriesFailed(Fooyin::RadioBrowser::RadioCategoryType type, const QString& error);
    void stationLookupFinished(int requestId, const Fooyin::RadioBrowser::RadioStationList& stations);
    void stationLookupFailed(int requestId, const QString& error);

private:
    enum class StationRequestType : uint8_t
    {
        None = 0,
        Search,
        Endpoint,
    };

    struct StationRequestContext
    {
        StationRequestType type{StationRequestType::None};
        RadioSearchRequest searchRequest;
        QString endpointPath;
        int limit{100};
        int offset{0};
        int apiIndex{0};
        int attempts{1};
    };

    struct CategoryRequestContext
    {
        RadioCategoryType type{RadioCategoryType::Country};
        int apiIndex{0};
        int attempts{1};
    };

    struct StationLookupRequestContext
    {
        int requestId{0};
        QStringList uuids;
        QString url;
        int apiIndex{0};
        int attempts{1};
    };

    enum class PendingRequestType : uint8_t
    {
        Station = 0,
        Category,
        Lookup,
        Click,
    };

    struct PendingApiRequest
    {
        PendingRequestType type{PendingRequestType::Station};
        StationRequestContext station;
        CategoryRequestContext category;
        StationLookupRequestContext lookup;
        QString stationUuid;
    };

    void startSearchRequest(const RadioSearchRequest& request, int apiIndex, int attempts = 1);
    void startEndpointRequest(const QString& path, int limit, int offset, int apiIndex, int attempts = 1);
    void startCategoryRequest(RadioCategoryType type, int apiIndex, int attempts = 1);
    void startStationLookupRequest(const StationLookupRequestContext& context);
    void startClickRequest(const QString& stationUuid);
    void queueOrStart(const PendingApiRequest& request);
    void startPendingRequest(const PendingApiRequest& request);
    void startApiServerLookup();
    void handleApiServerLookup();
    void handleReply();
    void handleStationLookupReply();
    void fetchStationsEndpoint(const QString& path, const QString& label, int limit, int offset);
    void handleCategoriesReply(RadioCategoryType type);
    [[nodiscard]] QString stationRequestText() const;

    [[nodiscard]] QString apiBase(int index) const;
    [[nodiscard]] bool retryStationRequest();
    [[nodiscard]] bool retryStationLookupRequest();
    [[nodiscard]] bool retryCategoryRequest(RadioCategoryType type);
    [[nodiscard]] QUrl buildSearchUrl(const RadioSearchRequest& request, int apiIndex) const;
    [[nodiscard]] QUrl buildStationsEndpointUrl(const QString& path, int limit, int offset, int apiIndex) const;
    [[nodiscard]] QUrl buildStationLookupUrl(const QStringList& uuids, int apiIndex) const;
    [[nodiscard]] QUrl buildStationUrlLookupUrl(const QString& stationUrl, int apiIndex) const;
    [[nodiscard]] QUrl buildCategoryUrl(RadioCategoryType type, int apiIndex) const;
    [[nodiscard]] QUrl buildClickUrl(const QString& stationUuid) const;

    std::shared_ptr<NetworkAccessManager> m_network;
    SettingsManager* m_settings;

    int m_apiIndex;
    QStringList m_apiBases;
    bool m_apiDiscoveryFinished;

    QPointer<QDnsLookup> m_apiLookup;
    QPointer<QNetworkReply> m_reply;
    QPointer<QNetworkReply> m_lookupReply;
    QPointer<QNetworkReply> m_clickReply;

    std::optional<StationRequestContext> m_stationRequest;
    std::optional<StationLookupRequestContext> m_lookupRequest;
    std::optional<StationRequestContext> m_pendingStationRequest;
    std::optional<StationLookupRequestContext> m_pendingLookupRequest;
    std::optional<QString> m_pendingClickRequest;

    std::unordered_map<RadioCategoryType, QPointer<QNetworkReply>> m_categoryReplies;
    std::unordered_map<RadioCategoryType, CategoryRequestContext> m_categoryRequests;
    std::unordered_map<RadioCategoryType, CategoryRequestContext> m_pendingCategoryRequests;
};
} // namespace RadioBrowser
} // namespace Fooyin
