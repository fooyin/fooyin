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

#include "fieldregistry.h"

namespace {
void loadDefaults(Fooyin::Filters::FieldRegistry* registry)
{
    registry->addItem({.id = 0, .index = 0, .name = "Genre", .field = "%<genre>%", .sortField = ""});
    registry->addItem({.id = 1, .index = 1, .name = "Album Artist", .field = "%albumartist%", .sortField = ""});
    registry->addItem({.id = 2, .index = 2, .name = "Artist", .field = "%<artist>%", .sortField = ""});
    registry->addItem({.id = 3, .index = 3, .name = "Album", .field = "%album%", .sortField = ""});
}
} // namespace

namespace Fooyin::Filters {
FieldRegistry::FieldRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        const auto field = itemById(id);
        emit fieldChanged(field);
    });
}

void FieldRegistry::loadItems()
{
    ItemRegistry::loadItems();

    if(m_items.empty()) {
        loadDefaults(this);
    }
}
} // namespace Fooyin::Filters

#include "moc_fieldregistry.cpp"
