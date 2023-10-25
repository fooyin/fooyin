/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#pragma once

#include <utils/settings/settingspage.h>
#include <utils/id.h>

#include <QIcon>

class QTabWidget;

namespace Fy::Utils {
class SettingsPage;
using PageList = std::vector<SettingsPage*>;

struct SettingsCategory
{
    Id id;
    QString name;
    QTabWidget* tabWidget{nullptr};
    PageList pages{};
    int index{-1};

    bool operator==(const SettingsCategory& other) const
    {
        return id == other.id;
    }

    [[nodiscard]] int findPageById(const Id& idToFind) const
    {
        auto it = std::ranges::find_if(std::as_const(pages), [idToFind](const SettingsPage* page) {
            return page->id() == idToFind;
        });
        return it == pages.end() ? -1 : static_cast<int>(std::distance(pages.begin(), it));
    }
};
} // namespace Fy::Utils
