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

#include "filtersplugin.h"

#include "filtermanager.h"
#include "filtersettings.h"
#include "filterwidget.h"
#include "searchwidget.h"

#include <core/actions/actioncontainer.h>
#include <gui/widgetfactory.h>
#include <pluginsystem/pluginmanager.h>

#include <QMenu>

namespace Filters {
FiltersPlugin::FiltersPlugin() = default;

FiltersPlugin::~FiltersPlugin() = default;

void FiltersPlugin::initialise()
{
    m_filterManager  = new FilterManager(this);
    m_filterSettings = std::make_unique<Settings::FiltersSettings>();

    PluginSystem::addObject(m_filterManager);

    auto* factory = PluginSystem::object<Gui::Widgets::WidgetFactory>();
    factory->registerClass<FilterWidget>("Filter", {"Filter"});
    factory->registerClass<GenreFilter>("Genre", {"Filter"});
    factory->registerClass<YearFilter>("Year", {"Filter"});
    factory->registerClass<AlbumArtistFilter>("Album Artist", {"Filter"});
    factory->registerClass<ArtistFilter>("Artist", {"Filter"});
    factory->registerClass<AlbumFilter>("Album", {"Filter"});
    factory->registerClass<SearchWidget>("Search");
}
} // namespace Filters
