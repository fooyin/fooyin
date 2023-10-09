/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/library/sortingregistry.h>

namespace Fy::Core::Library {
void loadDefaults(SortingRegistry* registry)
{
    registry->addItem({.id     = 0,
                       .index  = 0,
                       .name   = "Album Artist/Year/Album/Disc/Track/Title",
                       .script = "%albumartist% - %year% - %album% - $num(%disc%,2) - $num(%track%,2) - %title%"});
    registry->addItem({.id = 1, .index = 1, .name = "Album Artist", .script = "%albumartist%"});
    registry->addItem({.id = 2, .index = 2, .name = "Album", .script = "%album%"});
    registry->addItem({.id = 3, .index = 3, .name = "Title", .script = "%title%"});
    registry->addItem({.id = 4, .index = 4, .name = "Track Number", .script = "%track%"});
}

SortingRegistry::SortingRegistry(Utils::SettingsManager* settings, QObject* parent)
    : ItemRegistry{settings, parent}
{
    QObject::connect(this, &Utils::RegistryBase::itemChanged, this, [this](int id) {
        const auto sort = itemById(id);
        emit sortChanged(sort);
    });
}

void SortingRegistry::loadItems()
{
    ItemRegistry::loadItems();

    if(m_items.empty()) {
        loadDefaults(this);
    }
}
} // namespace Fy::Core::Library
