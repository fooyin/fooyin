/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "filtercolumnregistry.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
FilterColumnRegistry::FilterColumnRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{u"Filters/FilterColumns"_s, settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        if(const auto field = itemById(id)) {
            emit columnChanged(field.value());
        }
    });

    loadItems();
}

void FilterColumnRegistry::loadDefaults()
{
    addDefaultItem({.name = tr("Genre"), .field = u"%<genre>%"_s});
    addDefaultItem({.name = tr("Album Artist"), .field = u"%<albumartist>%"_s});
    addDefaultItem({.name = tr("Artist"), .field = u"%<artist>%"_s});
    addDefaultItem({.name = tr("Album"), .field = u"%album%"_s});
    addDefaultItem({.name = tr("Date"), .field = u"%date%"_s});
}
} // namespace Fooyin::Filters

#include "moc_filtercolumnregistry.cpp"
