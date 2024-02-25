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

#include "librarytreegroupregistry.h"

namespace Fooyin {
LibraryTreeGroupRegistry::LibraryTreeGroupRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{QStringLiteral("LibraryTree/LibraryTreeGroups"), settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        const auto grouping = itemById(id);
        emit groupingChanged(grouping);
    });

    loadItems();
}

void LibraryTreeGroupRegistry::loadDefaults()
{
    addDefaultItem(
        {.id     = 0,
         .index  = 0,
         .name   = tr("Artist/Album"),
         .script = QStringLiteral("$if2(%albumartist%,%artist%)||%album% (%year%)||%disc%.$num(%track%,2). %title%")});
    addDefaultItem({.id     = 1,
                    .index  = 1,
                    .name   = tr("Album"),
                    .script = QStringLiteral("%album% (%year%)||%disc%.$num(%track%,2). %title%")});
    addDefaultItem({.id     = 2,
                    .index  = 2,
                    .name   = tr("Folder Structure"),
                    .script = QStringLiteral("$replace(%relativepath%,/,||)")});
}
} // namespace Fooyin

#include "moc_librarytreegroupregistry.cpp"
