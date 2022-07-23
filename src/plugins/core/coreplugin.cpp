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
#include "coreplugin.h"

#include "app/application.h"
#include "gui/controls/controlwidget.h"
#include "gui/filter/filterwidget.h"
#include "gui/info/infowidget.h"
#include "gui/library/coverwidget.h"
#include "gui/library/searchwidget.h"
#include "gui/library/statuswidget.h"
#include "gui/playlist/playlistwidget.h"
#include "gui/widgets/spacer.h"
#include "gui/widgets/splitterwidget.h"
#include "utils/utils.h"

CorePlugin::CorePlugin() = default;

CorePlugin::~CorePlugin() = default;

void CorePlugin::initialise()
{
    Util::factory()->registerClass<ControlWidget>("Controls");

    Util::factory()->registerClass<Library::GenreFilter>("Genre", {"Filter"});
    Util::factory()->registerClass<Library::YearFilter>("Year", {"Filter"});
    Util::factory()->registerClass<Library::AlbmArtistFilter>("AlbumArtist", {"Filter"});
    Util::factory()->registerClass<Library::ArtistFilter>("Artist", {"Filter"});
    Util::factory()->registerClass<Library::AlbumFilter>("Album", {"Filter"});

    Util::factory()->registerClass<InfoWidget>("Info");
    Util::factory()->registerClass<CoverWidget>("Artwork");
    Util::factory()->registerClass<SearchWidget>("Search");
    Util::factory()->registerClass<Library::PlaylistWidget>("Playlist");
    Util::factory()->registerClass<Widgets::Spacer>("Spacer");
    Util::factory()->registerClass<SplitterWidget>("Splitter");
    Util::factory()->registerClass<StatusWidget>("Status");
}

void CorePlugin::pluginsInitialised()
{
    m_app = new Application(this);
}
