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

#include "quicktagger.h"

#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

using namespace Qt::StringLiterals;

constexpr auto NameKey   = "name"_L1;
constexpr auto FieldKey  = "field"_L1;
constexpr auto IdKey     = "id"_L1;
constexpr auto ValuesKey = "values"_L1;
constexpr auto ValueKey  = "value"_L1;

constexpr auto DefaultRatingTagId = "Rating"_L1;

namespace Fooyin::QuickTagger {
namespace {
QString normaliseField(const QString& field)
{
    return field.trimmed();
}

QString generateId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QByteArray quickTagsJson(const std::vector<QuickTag>& tags)
{
    QJsonArray array;

    for(const auto& tag : tags) {
        QJsonArray values;
        for(const auto& value : tag.values) {
            values.append(QJsonObject{{IdKey, value.id}, {ValueKey, value.value}});
        }
        array.append(QJsonObject{
            {IdKey, tag.id}, {NameKey, quickTagDisplayName(tag)}, {FieldKey, tag.field}, {ValuesKey, values}});
    }

    return QJsonDocument{array}.toJson(QJsonDocument::Compact);
}
} // namespace

QString quickTagDisplayName(const QuickTag& tag)
{
    return tag.name.isEmpty() ? tag.field : tag.name;
}

std::vector<QuickTag> quickTags(const SettingsManager& settings)
{
    const auto document = QJsonDocument::fromJson(settings.value<Settings::QuickTagger::Fields>());
    std::vector<QuickTag> tags;

    const QJsonArray tagArray = document.array();
    for(qsizetype i{0}; i < tagArray.size(); ++i) {
        const QJsonValue item = tagArray.at(i);
        const auto object     = item.toObject();

        QuickTag tag;
        tag.id    = object.value(IdKey).toString().trimmed();
        tag.field = normaliseField(object.value(FieldKey).toString());
        tag.name  = object.value(NameKey).toString().trimmed();

        if(tag.id.isEmpty()) {
            tag.id = generateId();
        }
        if(tag.name.isEmpty()) {
            tag.name = tag.field;
        }

        const QJsonArray valueArray = object.value(ValuesKey).toArray();

        for(qsizetype j{0}; j < valueArray.size(); ++j) {
            const QJsonValue value = valueArray.at(j);
            QuickTagValue tagValue;

            if(value.isObject()) {
                const auto valueObject = value.toObject();
                tagValue.id            = valueObject.value(IdKey).toString().trimmed();
                tagValue.value         = valueObject.value(ValueKey).toString().trimmed();
            }
            else {
                tagValue.value = value.toString().trimmed();
            }

            if(!tagValue.value.isEmpty()) {
                if(tagValue.id.isEmpty()) {
                    tagValue.id = generateId();
                }
                tag.values.emplace_back(tagValue);
            }
        }
        if(!tag.field.isEmpty() && !tag.values.empty()) {
            tags.emplace_back(tag);
        }
    }

    return tags;
}

void setQuickTags(SettingsManager& settings, const std::vector<QuickTag>& tags)
{
    settings.set<Settings::QuickTagger::Fields>(quickTagsJson(tags));
}

std::vector<QuickTag> defaultQuickTags()
{
    QuickTag rating;
    rating.id    = DefaultRatingTagId;
    rating.name  = QCoreApplication::translate("QuickTagger", "Rating");
    rating.field = u"rating"_s;

    for(int value{1}; value <= 5; ++value) {
        const QString text = QString::number(value);
        rating.values.push_back({.id = text, .value = text});
    }

    return {std::move(rating)};
}

QByteArray defaultQuickTagsJson()
{
    return quickTagsJson(defaultQuickTags());
}

int quickTaggerConfirmationThreshold(const SettingsManager& settings)
{
    return settings.value<Settings::QuickTagger::ConfirmationThreshold>();
}

void setQuickTaggerConfirmationThreshold(SettingsManager& settings, int threshold)
{
    settings.set<Settings::QuickTagger::ConfirmationThreshold>(std::max(0, threshold));
}
} // namespace Fooyin::QuickTagger
