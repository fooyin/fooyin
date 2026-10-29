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

#include "libraryfilterregistry.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
LibraryFilterRegistry::LibraryFilterRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{u"Library/FilterPresets"_s, settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        if(const auto field = itemById(id)) {
            Q_EMIT libraryFilterChanged(field.value());
        }
    });

    loadItems();
}
} // namespace Fooyin::Filters

#include "moc_libraryfilterregistry.cpp"
