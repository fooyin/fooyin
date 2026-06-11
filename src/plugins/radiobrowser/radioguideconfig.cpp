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

#include "radioguideconfig.h"

#include <utils/settings/settingsmanager.h>

#include <QDataStream>
#include <QIODevice>

using namespace Qt::StringLiterals;

constexpr auto GuideTagsKey          = "RadioBrowser/GuideTags";
constexpr auto GuideShowCountriesKey = "RadioBrowser/GuideShowCountries";
constexpr auto GuideStartupEntryKey  = "RadioBrowser/GuideStartupEntry";

namespace Fooyin::RadioBrowser {
namespace {
RadioGuideTagList flatten(const RadioGuideTagSections& sections)
{
    RadioGuideTagList tags;
    for(const RadioGuideTagSection& section : sections) {
        tags.insert(tags.cend(), section.tags.cbegin(), section.tags.cend());
    }
    return tags;
}

RadioGuideTagSections normaliseSections(const RadioGuideTagSections& sections)
{
    RadioGuideTagSections result;

    for(const RadioGuideTagSection& section : sections) {
        RadioGuideTagSection entry{.name = section.name.trimmed(), .tags = {}};
        if(entry.name.isEmpty()) {
            continue;
        }

        for(const RadioGuideTag& tag : section.tags) {
            RadioGuideTag entryTag{.name = tag.name.trimmed(), .tag = tag.tag.trimmed()};
            if(!entryTag.name.isEmpty() && !entryTag.tag.isEmpty()) {
                entry.tags.emplace_back(std::move(entryTag));
            }
        }

        result.emplace_back(std::move(entry));
    }

    return result;
}

RadioGuideConfig configFromSettings(const SettingsManager& settings, RadioGuideTagSections sections)
{
    return {
        .sections        = normaliseSections(sections),
        .showCountries   = settings.fileValue(GuideShowCountriesKey, true).toBool(),
        .startupEntryKey = settings.fileValue(GuideStartupEntryKey).toString().trimmed(),
    };
}
} // namespace

RadioGuideTagList RadioGuideConfigStore::allTags()
{
    return flatten(defaultTags());
}

RadioGuideTagSections RadioGuideConfigStore::defaultTags()
{
    return {
        {.name = tr("Genres"),
         .tags = {{.name = tr("Blues"), .tag = u"blues"_s},
                  {.name = tr("Classical"), .tag = u"classical"_s},
                  {.name = tr("Country"), .tag = u"country"_s},
                  {.name = tr("Dance"), .tag = u"dance"_s},
                  {.name = tr("Disco"), .tag = u"disco"_s},
                  {.name = tr("Easy"), .tag = u"easy listening"_s},
                  {.name = tr("Folk"), .tag = u"folk"_s},
                  {.name = tr("Hits"), .tag = u"hits"_s},
                  {.name = tr("Jazz"), .tag = u"jazz"_s},
                  {.name = tr("Oldies"), .tag = u"oldies"_s},
                  {.name = tr("Pop"), .tag = u"pop"_s},
                  {.name = tr("Rap"), .tag = u"rap"_s},
                  {.name = tr("Rock"), .tag = u"rock"_s},
                  {.name = tr("Soul"), .tag = u"soul"_s}}},

        {.name = tr("More Genres"),
         .tags = {{.name = tr("Alternative"), .tag = u"alternative"_s},
                  {.name = tr("Ambient"), .tag = u"ambient"_s},
                  {.name = tr("Club"), .tag = u"club"_s},
                  {.name = tr("Electronic"), .tag = u"electronic"_s},
                  {.name = tr("Funk"), .tag = u"funk"_s},
                  {.name = tr("Hip Hop"), .tag = u"hiphop"_s},
                  {.name = tr("House"), .tag = u"house"_s},
                  {.name = tr("Indie"), .tag = u"indie"_s},
                  {.name = tr("Latino"), .tag = u"latino"_s},
                  {.name = tr("Metal"), .tag = u"metal"_s},
                  {.name = tr("Punk"), .tag = u"punk"_s},
                  {.name = tr("Reggae"), .tag = u"reggae"_s},
                  {.name = tr("Salsa"), .tag = u"salsa"_s},
                  {.name = tr("World Music"), .tag = u"world music"_s}}},

        {.name = tr("Eras"),
         .tags = {{.name = tr("40s"), .tag = u"40s"_s},
                  {.name = tr("50s"), .tag = u"50s"_s},
                  {.name = tr("60s"), .tag = u"60s"_s},
                  {.name = tr("70s"), .tag = u"70s"_s},
                  {.name = tr("80s"), .tag = u"80s"_s},
                  {.name = tr("90s"), .tag = u"90s"_s},
                  {.name = tr("2000s"), .tag = u"2000s"_s},
                  {.name = tr("2010s"), .tag = u"2010s"_s}}},

        {.name = tr("Talk"),
         .tags = {{.name = tr("College Radio"), .tag = u"college radio"_s},
                  {.name = tr("Community Radio"), .tag = u"community radio"_s},
                  {.name = tr("Culture"), .tag = u"culture"_s},
                  {.name = tr("News"), .tag = u"news"_s},
                  {.name = tr("Public Radio"), .tag = u"public radio"_s},
                  {.name = tr("Religion"), .tag = u"religious"_s},
                  {.name = tr("Sport"), .tag = u"sports"_s},
                  {.name = tr("Talk"), .tag = u"talk"_s}}},
    };
}

RadioGuideConfig RadioGuideConfigStore::defaultConfig()
{
    return {.sections = defaultTags(), .showCountries = true, .startupEntryKey = {}};
}

RadioGuideConfig RadioGuideConfigStore::fromSettings(const SettingsManager& settings)
{
    const QByteArray compressed = settings.fileValue(GuideTagsKey).toByteArray();
    if(compressed.isEmpty()) {
        return configFromSettings(settings, defaultTags());
    }

    QByteArray data = qUncompress(compressed);
    if(data.isEmpty()) {
        return configFromSettings(settings, defaultTags());
    }

    QDataStream stream{&data, QIODevice::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 sectionCount{0};
    stream >> sectionCount;
    if(stream.status() != QDataStream::Ok || sectionCount < 0 || sectionCount > 1000) {
        return configFromSettings(settings, defaultTags());
    }

    RadioGuideTagSections sections;
    sections.reserve(static_cast<size_t>(sectionCount));

    for(quint32 sectionIndex{0}; sectionIndex < sectionCount; ++sectionIndex) {
        RadioGuideTagSection section;
        quint32 tagCount{0};
        stream >> section.name >> tagCount;
        if(stream.status() != QDataStream::Ok || tagCount < 0 || tagCount > 10000) {
            return configFromSettings(settings, defaultTags());
        }

        section.tags.reserve(static_cast<size_t>(tagCount));

        for(quint32 tagIndex{0}; tagIndex < tagCount; ++tagIndex) {
            RadioGuideTag tag;
            stream >> tag.name >> tag.tag;
            if(stream.status() != QDataStream::Ok) {
                return configFromSettings(settings, defaultTags());
            }

            section.tags.emplace_back(std::move(tag));
        }

        sections.emplace_back(std::move(section));
    }

    return configFromSettings(settings, std::move(sections));
}

void RadioGuideConfigStore::save(SettingsManager& settings, const RadioGuideConfig& config)
{
    RadioGuideTagSections entries = normaliseSections(config.sections);

    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint32>(entries.size());

    for(const RadioGuideTagSection& section : entries) {
        stream << section.name << static_cast<quint32>(section.tags.size());

        for(const RadioGuideTag& tag : section.tags) {
            stream << tag.name << tag.tag;
        }
    }

    settings.fileSet(GuideTagsKey, qCompress(data, 9));
    settings.fileSet(GuideShowCountriesKey, config.showCountries);
    settings.fileSet(GuideStartupEntryKey, config.startupEntryKey.trimmed());
}

void RadioGuideConfigStore::reset(SettingsManager& settings)
{
    settings.fileRemove(GuideTagsKey);
    settings.fileRemove(GuideShowCountriesKey);
    settings.fileRemove(GuideStartupEntryKey);
}
} // namespace Fooyin::RadioBrowser
