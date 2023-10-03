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

#include "filtermanager.h"

#include "fieldregistry.h"
#include "filterstore.h"
#include "filterwidget.h"

#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>

#include <utils/async.h>

#include <QActionGroup>
#include <QMenu>

namespace Fy::Filters {
bool containsSearch(const QString& text, const QString& search)
{
    return text.contains(search, Qt::CaseInsensitive);
}

// TODO: Support user-defined tags
bool matchSearch(const Core::Track& track, const QString& search)
{
    if(search.isEmpty()) {
        return true;
    }

    const auto artists = track.artists();
    for(const QString& artist : artists) {
        if(artist.contains(search, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return (containsSearch(track.title(), search) || containsSearch(track.album(), search)
            || containsSearch(track.albumArtist(), search));
}

Core::TrackList filterTracks(const Core::TrackList& tracks, const QString& search)
{
    auto filteredTracks = Utils::filter(tracks, [search](const Core::Track& track) {
        return matchSearch(track, search);
    });
    return filteredTracks;
}

struct FilterManager::Private : QObject
{
    Core::Library::MusicLibrary* library;
    Core::Playlist::PlaylistHandler* playlistHandler;
    Gui::TrackSelectionController* trackSelection;
    Utils::SettingsManager* settings;

    FieldRegistry fieldsRegistry;
    Core::TrackList filteredTracks;
    FilterStore filterStore;
    QString searchFilter;

    Private(Core::Library::MusicLibrary* library, Core::Playlist::PlaylistHandler* playlistHandler,
            Gui::TrackSelectionController* trackSelection, Utils::SettingsManager* settings)
        : library{library}
        , playlistHandler{playlistHandler}
        , trackSelection{trackSelection}
        , settings{settings}
        , fieldsRegistry{settings}
    {
        fieldsRegistry.loadItems();
    }
};

FilterManager::FilterManager(Core::Library::MusicLibrary* library, Core::Playlist::PlaylistHandler* playlistHandler,
                             Gui::TrackSelectionController* trackSelection, Utils::SettingsManager* settings,
                             QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(library, playlistHandler, trackSelection, settings)}
{
    QObject::connect(p->library, &Core::Library::MusicLibrary::tracksAdded, this, &FilterManager::tracksAdded);
    QObject::connect(p->library, &Core::Library::MusicLibrary::tracksUpdated, this, &FilterManager::tracksUpdated);
    QObject::connect(p->library, &Core::Library::MusicLibrary::tracksDeleted, this, &FilterManager::tracksRemoved);

    const auto tracksChanged = [this]() {
        getFilteredTracks();
        emit filteredItems(-1);
    };

    QObject::connect(p->library, &Core::Library::MusicLibrary::tracksLoaded, this, tracksChanged);
    QObject::connect(p->library, &Core::Library::MusicLibrary::tracksSorted, this, tracksChanged);
    QObject::connect(p->library, &Core::Library::MusicLibrary::libraryChanged, this, tracksChanged);
    QObject::connect(p->library, &Core::Library::MusicLibrary::libraryRemoved, this, tracksChanged);

    QObject::connect(&p->fieldsRegistry, &FieldRegistry::fieldChanged, this, &FilterManager::fieldChanged);
}

FilterWidget* FilterManager::createFilter()
{
    return new FilterWidget(this, p->trackSelection, p->settings);
}

FilterManager::~FilterManager()
{
    p->fieldsRegistry.saveItems();
}

void FilterManager::shutdown()
{
    p->fieldsRegistry.saveItems();
};

Core::TrackList FilterManager::tracks() const
{
    return hasTracks() ? p->filteredTracks : p->library->tracks();
}

bool FilterManager::hasTracks() const
{
    return !p->filteredTracks.empty() || !p->searchFilter.isEmpty() || p->filterStore.hasActiveFilters();
}

FieldRegistry* FilterManager::fieldRegistry() const
{
    return &p->fieldsRegistry;
}

LibraryFilter* FilterManager::registerFilter(const QString& name)
{
    const FilterField filterField = p->fieldsRegistry.itemByName(name);
    return p->filterStore.addFilter(filterField);
}

void FilterManager::unregisterFilter(int index)
{
    p->filterStore.removeFilter(index);
    emit filteredItems(-1);
    getFilteredTracks();
}

void FilterManager::changeFilter(int index)
{
    p->filterStore.clearActiveFilters(index, true);
    getFilteredTracks();
    emit filteredItems(index - 1);
}

FilterField FilterManager::findField(const QString& name) const
{
    return p->fieldsRegistry.itemByName(name);
}

void FilterManager::getFilteredTracks()
{
    p->filteredTracks.clear();

    for(auto& filter : p->filterStore.activeFilters()) {
        if(p->filteredTracks.empty()) {
            std::ranges::copy(filter.tracks, std::back_inserter(p->filteredTracks));
        }
        else {
            p->filteredTracks
                = Utils::intersection<Core::Track, Core::Track::TrackHash>(filter.tracks, p->filteredTracks);
        }
    }
}

void FilterManager::selectionChanged(int index)
{
    p->filterStore.clearActiveFilters(index);
    getFilteredTracks();
    emit filteredItems(index);
}

void FilterManager::searchChanged(const QString& search)
{
    const bool reset = p->searchFilter.length() > search.length();
    p->searchFilter  = search;

    const auto tracks = Utils::asyncExec<Core::TrackList>([this, reset]() {
        return filterTracks(!reset && !p->filteredTracks.empty() ? p->filteredTracks : p->library->tracks(),
                            p->searchFilter);
    });

    p->filteredTracks = tracks;
    emit filteredItems(-1);
}

QMenu* FilterManager::filterHeaderMenu(int index, FilterField* field)
{
    auto* menu = new QMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* filterList = new QActionGroup{menu};

    for(const auto& [filterIndex, registryField] : p->fieldsRegistry.items()) {
        const QString name = registryField.name;
        auto* fieldAction  = new QAction(menu);
        fieldAction->setText(name);
        fieldAction->setData(name);
        fieldAction->setCheckable(true);
        fieldAction->setChecked(field && name == field->name);
        menu->addAction(fieldAction);
        filterList->addAction(fieldAction);
    }

    menu->setDefaultAction(filterList->checkedAction());
    QObject::connect(filterList, &QActionGroup::triggered, this, [this, index](QAction* action) {
        emit filterChanged(index, action->data().toString());
    });

    return menu;
}
} // namespace Fy::Filters
