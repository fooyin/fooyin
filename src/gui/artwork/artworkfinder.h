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

#pragma once

#include "fygui_export.h"

#include "sources/artworksource.h"

#include <core/scripting/scriptparser.h>
#include <core/track.h>

namespace Fooyin {
struct ArtworkResult;
class ArtworkSource;
class NetworkAccessManager;
class SettingsManager;

class FYGUI_EXPORT ArtworkFinder : public QObject
{
    Q_OBJECT

public:
    ArtworkFinder(std::shared_ptr<NetworkAccessManager> networkManager, SettingsManager* settings,
                  QObject* parent = nullptr);

    void findArtwork(Track::Cover type, const QString& artist, const QString& album, const QString& title = {});

    [[nodiscard]] std::vector<ArtworkSource*> sources() const;

    void saveState();
    void restoreState();
    void sort();
    void reset();

signals:
    void coverFound(const Fooyin::SearchResult& result);
    void coverLoaded(const QUrl& url, const Fooyin::ArtworkResult& result);
    void coverLoadProgress(const QUrl& url, int progress);
    void coverLoadError(const QUrl& url);
    void searchFinished();

private:
    void loadDefaults();
    void finishOrStartNextSource(bool forceFinish = false);
    bool findNextAvailableSource();
    void onSearchResults(const SearchResults& results);
    void onDownloadProgress(const QUrl& url, qint64 bytesReceived, qint64 bytesTotal);
    void onImageResult(const QUrl& url, QNetworkReply* reply);

    std::shared_ptr<NetworkAccessManager> m_networkManager;
    SettingsManager* m_settings;

    std::vector<ArtworkSource*> m_sources;

    ScriptParser m_parser;
    SearchParams m_params;
    int m_currentSourceIndex;
    ArtworkSource* m_currentSource;
    std::vector<QNetworkReply*> m_downloads;
};
} // namespace Fooyin
