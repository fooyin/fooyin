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

#pragma once

#include "fycore_export.h"

#include <core/coresettings.h>
#include <core/library/librarysort.h>
#include <utils/itemregistry.h>

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Core::Library {
class FYCORE_EXPORT SortingRegistry : public Utils::ItemRegistry<Sorting::SortScript, Settings::LibrarySorting>
{
    Q_OBJECT

public:
    explicit SortingRegistry(Utils::SettingsManager* settings, QObject* parent = nullptr);

    void loadItems() override;

signals:
    void sortChanged(const Sorting::SortScript& preset);
};
} // namespace Core::Library
} // namespace Fy
