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

#include "radiodiscovery.h"

#include <QLocale>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
QString displayNameForCountry(const QString& countryName, const QString& countryCode)
{
    const QString code = countryCode.trimmed().toUpper();
    if(!code.isEmpty()) {
        const QLocale::Territory territory = QLocale::codeToTerritory(code);
        if(territory != QLocale::AnyTerritory) {
            return QLocale::territoryToString(territory);
        }
    }

    QString name = countryName.trimmed();
    if(name.startsWith("The "_L1, Qt::CaseInsensitive)) {
        name = name.sliced(4).trimmed();
    }

    return name;
}
} // namespace Fooyin::RadioBrowser
