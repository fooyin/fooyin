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

using namespace Qt::StringLiterals;

namespace Fooyin {
SortingRegistry::SortingRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{u"Library/LibrarySorting"_s, settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        if(const auto sort = itemById(id)) {
            emit sortChanged(sort.value());
        }
    });

    loadItems();
}

void SortingRegistry::loadDefaults()
{
    addDefaultItem({.name = tr("Album"), .script = u"%album% - $num(%disc%,2) - $num(%track%,2)"_s});
    addDefaultItem({.name = tr("Artist"), .script = u"%artist% - %date% - $num(%disc%,2) - $num(%track%,2)"_s});
    addDefaultItem({.name = tr("Title"), .script = u"%title%"_s});
    addDefaultItem({.name = tr("Track Number"), .script = u"$num(%disc%,2) - $num(%track%,2)"_s});
}
} // namespace Fooyin

#include "moc_sortingregistry.cpp"
