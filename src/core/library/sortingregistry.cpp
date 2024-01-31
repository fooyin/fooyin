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

#include "sortingregistry.h"

namespace Fooyin {
SortingRegistry::SortingRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{LibrarySorting, settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        const auto sort = itemById(id);
        emit sortChanged(sort);
    });

    loadItems();
}

void SortingRegistry::loadDefaults()
{
    addDefaultItem({.id = 0, .index = 0, .name = "Album", .script = "%album% - $num(%disc%,2) - $num(%track%,2)"});
    addDefaultItem(
        {.id = 1, .index = 1, .name = "Artist", .script = "%artist% - %date% - $num(%disc%,2) - $num(%track%,2)"});
    addDefaultItem({.id = 2, .index = 2, .name = "Title", .script = "%title%"});
    addDefaultItem({.id = 3, .index = 3, .name = "Track Number", .script = "$num(%disc%,2) - $num(%track%,2)"});
}
} // namespace Fooyin

#include "moc_sortingregistry.cpp"
