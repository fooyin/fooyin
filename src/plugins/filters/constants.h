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

#include <Qt>

namespace Fy::Filters::Constants {
namespace Role {
constexpr auto Title   = Qt::UserRole + 1;
constexpr auto Tracks  = Qt::UserRole + 2;
constexpr auto Sorting = Qt::UserRole + 3;
} // namespace Role

namespace Icons::Category {
constexpr auto Filters = "://icons/category-filters.svg";
} // namespace Icons::Category

namespace Page {
constexpr auto FiltersGeneral = "Fooyin.Page.Filters.General";
constexpr auto FiltersFields  = "Fooyin.Page.Filters.Fields";
} // namespace Page
} // namespace Fy::Filters::Constants
