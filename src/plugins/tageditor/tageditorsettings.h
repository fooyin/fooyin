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

#include <QStringList>

namespace Fooyin {
class SettingsManager;

namespace TagEditor::SettingsKeys {
constexpr auto MultiValueSeparators         = "TagEditor/MultiValueSeparators";
constexpr auto AutoFillMultiValueSeparators = "TagEditor/AutoFillMultiValueSeparators";
constexpr auto DefaultMultiValueSeparators  = ";";
} // namespace TagEditor::SettingsKeys

namespace TagEditor {
[[nodiscard]] QStringList normaliseMultiValueSeparators(const QString& separators);
[[nodiscard]] QString multiValueSeparatorsText(const SettingsManager& settings);
[[nodiscard]] QStringList multiValueSeparators(const SettingsManager& settings);
[[nodiscard]] QStringList autoFillMultiValueSeparators(const SettingsManager& settings);
[[nodiscard]] QStringList splitMultiValueText(const QString& value, const QStringList& separators);
[[nodiscard]] QString joinMultiValueText(const QStringList& values, const QStringList& separators);
[[nodiscard]] QString primaryMultiValueSeparator(const QStringList& separators);
} // namespace TagEditor
} // namespace Fooyin
