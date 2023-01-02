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

#include "filtersplugin.h"

#include "filterwidget.h"

#include <QMenu>
#include <core/actions/actioncontainer.h>
#include <core/actions/actionmanager.h>
#include <core/constants.h>
#include <core/gui/mainwindow.h>
#include <core/widgets/widgetfactory.h>
#include <pluginsystem/pluginmanager.h>

FiltersPlugin::FiltersPlugin() = default;

FiltersPlugin::~FiltersPlugin() = default;

void FiltersPlugin::initialise() { }

void FiltersPlugin::finalise()
{
    auto* factory = PluginSystem::object<Widgets::WidgetFactory>();
    factory->registerClass<Library::FilterWidget>("Filter", {"Filter"});
    factory->registerClass<Library::GenreFilter>("Genre", {"Filter"});
    factory->registerClass<Library::YearFilter>("Year", {"Filter"});
    factory->registerClass<Library::AlbmArtistFilter>("AlbumArtist", {"Filter"});
    factory->registerClass<Library::ArtistFilter>("Artist", {"Filter"});
    factory->registerClass<Library::AlbumFilter>("Album", {"Filter"});
}
void FiltersPlugin::shutdown() { }
