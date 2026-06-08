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

#include "radiostation.h"

#include <core/constants.h>

#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
QString RadioStation::effectiveStreamUrl() const
{
    return !resolvedStreamUrl.isEmpty() ? resolvedStreamUrl : streamUrl;
}

QString RadioStation::searchResultKey() const
{
    return QStringList{
        uuid.trimmed(),        streamUrl.trimmed(), resolvedStreamUrl.trimmed(), effectiveStreamUrl().trimmed(),
        name.trimmed(),        codec.trimmed(),     QString::number(bitrate),    country.trimmed(),
        countryCode.trimmed(), language.trimmed()}
        .join(QLatin1StringView{Constants::UnitSeparator});
}

bool RadioStation::samePlayableStream(const RadioStation& other) const
{
    const QStringList lhsUrls{streamUrl.trimmed(), resolvedStreamUrl.trimmed(), effectiveStreamUrl().trimmed()};
    const QStringList rhsUrls{other.streamUrl.trimmed(), other.resolvedStreamUrl.trimmed(),
                              other.effectiveStreamUrl().trimmed()};

    for(const QString& lhsUrl : lhsUrls) {
        if(lhsUrl.isEmpty()) {
            continue;
        }

        for(const QString& rhsUrl : rhsUrls) {
            if(!rhsUrl.isEmpty() && lhsUrl == rhsUrl) {
                return true;
            }
        }
    }

    return false;
}

bool RadioStation::hasCompatibleDetails(const RadioStation& other) const
{
    const auto sameText = [](const QString& lhsText, const QString& rhsText) {
        return lhsText.trimmed().isEmpty() || rhsText.trimmed().isEmpty() || lhsText.trimmed() == rhsText.trimmed();
    };

    if(!sameText(name, other.name) || !sameText(codec, other.codec) || !sameText(country, other.country)
       || !sameText(countryCode, other.countryCode) || !sameText(language, other.language)) {
        return false;
    }

    return bitrate <= 0 || other.bitrate <= 0 || bitrate == other.bitrate;
}

bool RadioStation::sameCurrentStationIdentity(const RadioStation& other) const
{
    if(samePlayableStream(other)) {
        return hasCompatibleDetails(other);
    }

    const bool hasStreamUrl
        = !effectiveStreamUrl().trimmed().isEmpty() || !other.effectiveStreamUrl().trimmed().isEmpty();
    return !hasStreamUrl && !uuid.isEmpty() && uuid == other.uuid;
}

QStringList splitStationTags(const QString& tags)
{
    QStringList result;

    const auto splitTags = tags.split(u',', Qt::SkipEmptyParts);
    for(const QString& tag : splitTags) {
        const QString trimmed = tag.trimmed();
        if(!trimmed.isEmpty()) {
            result.emplace_back(trimmed);
        }
    }

    return result;
}

QJsonObject searchRequestToJson(const RadioSearchRequest& request)
{
    return {
        {u"text"_s, request.text},
        {u"countryCode"_s, request.countryCode},
        {u"language"_s, request.language},
        {u"tag"_s, request.tag},
        {u"codec"_s, request.codec},
        {u"bitrateMin"_s, request.bitrateMin},
        {u"bitrateMax"_s, request.bitrateMax},
        {u"order"_s, request.order},
        {u"reverse"_s, request.reverse},
        {u"hideBroken"_s, request.hideBroken},
    };
}

RadioSearchRequest searchRequestFromJson(const QJsonObject& object)
{
    RadioSearchRequest request;
    request.text        = object.value(u"text"_s).toString();
    request.countryCode = object.value(u"countryCode"_s).toString();
    request.language    = object.value(u"language"_s).toString();
    request.tag         = object.value(u"tag"_s).toString();
    request.codec       = object.value(u"codec"_s).toString();
    request.bitrateMin  = object.value(u"bitrateMin"_s).toInt();
    request.bitrateMax  = object.value(u"bitrateMax"_s).toInt();
    request.order       = object.value(u"order"_s).toString(u"clickcount"_s);
    request.reverse     = object.value(u"reverse"_s).toBool(true);
    request.hideBroken  = object.value(u"hideBroken"_s).toBool(true);
    return request;
}

bool sameSavedSearchRequest(const RadioSearchRequest& lhs, const RadioSearchRequest& rhs)
{
    return lhs.text.trimmed() == rhs.text.trimmed() && lhs.countryCode.trimmed() == rhs.countryCode.trimmed()
        && lhs.language.trimmed() == rhs.language.trimmed() && lhs.tag.trimmed() == rhs.tag.trimmed()
        && lhs.codec.trimmed() == rhs.codec.trimmed() && lhs.bitrateMin == rhs.bitrateMin
        && lhs.bitrateMax == rhs.bitrateMax && lhs.order.trimmed() == rhs.order.trimmed() && lhs.reverse == rhs.reverse
        && lhs.hideBroken == rhs.hideBroken;
}

bool isDefaultSearchRequest(const RadioSearchRequest& request)
{
    return request.text.trimmed().isEmpty() && request.countryCode.trimmed().isEmpty()
        && request.language.trimmed().isEmpty() && request.tag.trimmed().isEmpty() && request.codec.trimmed().isEmpty()
        && request.bitrateMin <= 0 && request.bitrateMax <= 0
        && (request.order.trimmed().isEmpty() || request.order.trimmed() == "clickcount"_L1) && request.reverse
        && request.hideBroken;
}
} // namespace Fooyin::RadioBrowser
