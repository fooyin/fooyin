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

#include "tageditorsettings.h"

#include <utils/settings/settingsmanager.h>

#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Fooyin::TagEditor {
namespace {
QString defaultSeparatorsText()
{
    return QString::fromLatin1(SettingsKeys::DefaultMultiValueSeparators);
}

QStringList defaultSeparators()
{
    return {defaultSeparatorsText()};
}
} // namespace

QStringList normaliseMultiValueSeparators(const QString& separators)
{
    QStringList values;
    const QStringList splitValues = separators.split(QRegularExpression{u"\\s+"_s}, Qt::SkipEmptyParts);
    for(const QString& value : splitValues) {
        if(!values.contains(value)) {
            values.append(value);
        }
    }

    return values.empty() ? defaultSeparators() : values;
}

QString multiValueSeparatorsText(const SettingsManager& settings)
{
    const QString value = settings.value(SettingsKeys::MultiValueSeparators).toString().trimmed();
    return value.isEmpty() ? defaultSeparatorsText() : value;
}

QStringList multiValueSeparators(const SettingsManager& settings)
{
    return normaliseMultiValueSeparators(multiValueSeparatorsText(settings));
}

QStringList autoFillMultiValueSeparators(const SettingsManager& settings)
{
    const QString value = settings.value(SettingsKeys::AutoFillMultiValueSeparators).toString().trimmed();
    return normaliseMultiValueSeparators(value.isEmpty() ? defaultSeparatorsText() : value);
}

QStringList splitMultiValueText(const QString& value, const QStringList& separators)
{
    QStringList effectiveSeparators
        = separators.empty() ? defaultSeparators() : normaliseMultiValueSeparators(separators.join(' '_L1));
    std::ranges::sort(effectiveSeparators, std::greater<>{}, &QString::size);

    QStringList values;
    QString current;

    for(qsizetype i{0}; i < value.size();) {
        bool matched{false};
        for(const QString& separator : std::as_const(effectiveSeparators)) {
            if(separator.isEmpty()) {
                continue;
            }
            if(value.sliced(i).startsWith(separator)) {
                values.append(current.trimmed());
                current.clear();
                i += separator.size();
                matched = true;
                break;
            }
        }

        if(!matched) {
            current.append(value.at(i));
            ++i;
        }
    }

    values.append(current.trimmed());
    values.removeAll(QString{});
    return values;
}

QString primaryMultiValueSeparator(const QStringList& separators)
{
    if(separators.empty()) {
        return defaultSeparatorsText();
    }

    return separators.front();
}

QString joinMultiValueText(const QStringList& values, const QStringList& separators)
{
    return values.join(primaryMultiValueSeparator(separators) + ' '_L1);
}
} // namespace Fooyin::TagEditor
