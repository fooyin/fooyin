/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "playlistcolumnregistry.h"

namespace {
void loadDefaults(Fooyin::PlaylistColumnRegistry* registry)
{
    registry->addItem({.id = 0, .index = 0, .name = "Track", .field = "$num(%track%,2)"});
    registry->addItem({.id = 1, .index = 1, .name = "Title", .field = "%title%"});
    registry->addItem({.id = 2, .index = 2, .name = "Artist", .field = "%artist%"});
    registry->addItem({.id = 3, .index = 3, .name = "Album Artist", .field = "%albumartist%"});
    registry->addItem({.id = 4, .index = 4, .name = "Album", .field = "%album%"});
    registry->addItem({.id = 5, .index = 5, .name = "Playcount", .field = "%playcount%"});
    registry->addItem({.id = 6, .index = 6, .name = "Duration", .field = "$timems(%duration%)"});
}
} // namespace

namespace Fooyin {
PlaylistColumnRegistry::PlaylistColumnRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{PlaylistColumns, settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        const auto column = itemById(id);
        emit columnChanged(column);
    });

    PlaylistColumnRegistry::loadItems();
}

void PlaylistColumnRegistry::loadItems()
{
    ItemRegistry::loadItems();

    if(m_items.empty()) {
        loadDefaults(this);
    }
}
} // namespace Fooyin
