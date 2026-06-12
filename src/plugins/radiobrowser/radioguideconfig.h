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

#include <QCoreApplication>
#include <QString>

#include <vector>

namespace Fooyin {
class SettingsManager;

namespace RadioBrowser {
struct RadioGuideTag
{
    QString name;
    QString tag;
};
using RadioGuideTagList = std::vector<RadioGuideTag>;

struct RadioGuideTagSection
{
    QString name;
    RadioGuideTagList tags;
};
using RadioGuideTagSections = std::vector<RadioGuideTagSection>;

struct RadioGuideConfig
{
    RadioGuideTagSections sections;
    bool showCountries{true};
    QString startupEntryKey;
};

class RadioGuideConfigStore
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::RadioBrowser::RadioGuideConfigStore)

public:
    [[nodiscard]] static RadioGuideTagList allTags();
    [[nodiscard]] static RadioGuideTagSections defaultTags();
    [[nodiscard]] static RadioGuideTagList defaultGenreTags();
    [[nodiscard]] static RadioGuideConfig defaultConfig();
    [[nodiscard]] static RadioGuideConfig fromSettings(const SettingsManager& settings);

    static void save(SettingsManager& settings, const RadioGuideConfig& config);
    static void reset(SettingsManager& settings);
};
} // namespace RadioBrowser
} // namespace Fooyin
