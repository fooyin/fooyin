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

#include <core/track.h>
#include <utils/settings/settingsentry.h>

#include <QByteArray>
#include <QObject>
#include <QString>

namespace Fooyin {
class SettingsManager;

namespace Settings::QuickTagger {
Q_NAMESPACE
enum QuickTaggerSettings : uint32_t
{
    Fields                = 1 | Type::ByteArray,
    ConfirmationThreshold = 2 | Type::Int,
};
Q_ENUM_NS(QuickTaggerSettings)
} // namespace Settings::QuickTagger

namespace QuickTagger {
struct QuickTagValue
{
    QString id;
    QString value;
};

struct QuickTag
{
    QString id;
    QString name;
    QString field;
    std::vector<QuickTagValue> values;
};

[[nodiscard]] std::vector<QuickTag> quickTags(const SettingsManager& settings);
void setQuickTags(SettingsManager& settings, const std::vector<QuickTag>& tags);
[[nodiscard]] std::vector<QuickTag> defaultQuickTags();
[[nodiscard]] QByteArray defaultQuickTagsJson();

[[nodiscard]] int quickTaggerConfirmationThreshold(const SettingsManager& settings);
void setQuickTaggerConfirmationThreshold(SettingsManager& settings, int threshold);

[[nodiscard]] QString quickTagDisplayName(const QuickTag& tag);
} // namespace QuickTagger
} // namespace Fooyin
