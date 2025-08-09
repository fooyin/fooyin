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

#include "artworksource.h"

namespace Fooyin {
class LastFmArtwork : public ArtworkSource
{
    Q_OBJECT

public:
    LastFmArtwork(NetworkAccessManager* network, SettingsManager* settings, int index, bool enabled,
                  QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] CoverTypes supportedTypes() const override;

    void search(const SearchParams& params) override;

private:
    void handleSearchReply();

    QString m_apiKey;
    QString m_secret;

    Track::Cover m_type;
    QString m_queryType;
};
} // namespace Fooyin
