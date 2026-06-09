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

#include <QMetaType>
#include <QString>
#include <QStringList>

#include <vector>

class QJsonObject;

namespace Fooyin::RadioBrowser {
enum StationColumn : uint8_t
{
    Station = 0,
    Country,
    Tags,
    Language,
    Codec,
    Bitrate,
    Votes,
    Clicks,
    Count,
};

struct RadioStation
{
    QString uuid;
    QString name;
    QString streamUrl;
    QString resolvedStreamUrl;
    QString homepage;
    QString favicon;
    QString tags;
    QString country;
    QString countryCode;
    QString language;
    QString codec;
    int bitrate{0};
    int clickCount{0};
    int voteCount{0};
    int playCount{0};
    qint64 lastPlayed{0};
    bool online{false};
    bool hls{false};
    bool local{false};

    [[nodiscard]] QString effectiveStreamUrl() const;
    [[nodiscard]] QString searchResultKey() const;

    [[nodiscard]] bool samePlayableStream(const RadioStation& other) const;
    [[nodiscard]] bool hasCompatibleDetails(const RadioStation& other) const;
    [[nodiscard]] bool sameCurrentStationIdentity(const RadioStation& other) const;
};
using RadioStationList = std::vector<RadioStation>;

struct RadioSearchRequest
{
    QString displayText;
    QString text;
    QString countryCode;
    QString language;
    QString tag;
    QString codec;
    int bitrateMin{0};
    int bitrateMax{0};
    QString order;
    bool reverse{true};
    bool hideBroken{true};
    int offset{0};
    int limit{100};
};

struct RadioSavedSearch
{
    QString id;
    QString name;
    RadioSearchRequest request;
};
using RadioSavedSearchList = std::vector<RadioSavedSearch>;

[[nodiscard]] QStringList splitStationTags(const QString& tags);
[[nodiscard]] QJsonObject searchRequestToJson(const RadioSearchRequest& request);
[[nodiscard]] RadioSearchRequest searchRequestFromJson(const QJsonObject& object);
[[nodiscard]] bool sameSavedSearchRequest(const RadioSearchRequest& lhs, const RadioSearchRequest& rhs);
[[nodiscard]] bool isDefaultSearchRequest(const RadioSearchRequest& request);
} // namespace Fooyin::RadioBrowser

Q_DECLARE_METATYPE(Fooyin::RadioBrowser::RadioStation)
Q_DECLARE_METATYPE(Fooyin::RadioBrowser::RadioStationList)
Q_DECLARE_METATYPE(Fooyin::RadioBrowser::RadioSavedSearch)
Q_DECLARE_METATYPE(Fooyin::RadioBrowser::RadioSavedSearchList)
