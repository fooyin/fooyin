/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "artworksource.h"

namespace Fooyin {
class DiscogsArtwork : public ArtworkSource
{
    Q_OBJECT

public:
    DiscogsArtwork(NetworkAccessManager* network, SettingsManager* settings, int index, bool enabled,
                   QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] CoverTypes supportedTypes() const override;

    void search(const SearchParams& params) override;

private:
    void handleSearchReply();
    void handleReleaseReply(uint64_t id);
    void handleArtistReply(const QStringList& artists, QNetworkReply* reply);
    void endSearchIfFinished();

    QString m_apiKey;
    QString m_secret;

    SearchParams m_params;

    std::map<uint64_t, QNetworkReply*> m_requests;
    SearchResults m_results;
};
} // namespace Fooyin
