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

#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>

#include <QActionGroup>
#include <QMenu>
#include <QThread>

namespace Fy::Filters {
constexpr auto FilterPlaylistName       = "Filter Results";
constexpr auto FilterPlaylistActiveName = "Filter Results (Playback)";

void sendToPlaylist(Core::Playlist::PlaylistHandler* playlistHandler, const Core::TrackList& filteredTracks)
{
    auto activePlaylist = playlistHandler->activePlaylist();
    if(!activePlaylist || activePlaylist->name() != FilterPlaylistName) {
        playlistHandler->createPlaylist(FilterPlaylistName, filteredTracks);
        return;
    }

    if(auto filterPlaylist = playlistHandler->playlistByName(FilterPlaylistActiveName)) {
        playlistHandler->exchangePlaylist(*filterPlaylist, *activePlaylist);
        playlistHandler->changeActivePlaylist(filterPlaylist->id());
    }
    else {
        playlistHandler->renamePlaylist(activePlaylist->id(), FilterPlaylistActiveName);
    }
    playlistHandler->createPlaylist(FilterPlaylistName, filteredTracks);
}

FilterManager::FilterManager(Core::Library::MusicLibrary* library, Core::Playlist::PlaylistHandler* playlistHandler,
                             FieldRegistry* fieldsRegistry, QObject* parent)
    : QObject{parent}
    , m_library{library}
    , m_playlistHandler{playlistHandler}
    , m_searchThread{new QThread(this)}
    , m_fieldsRegistry{fieldsRegistry}
{
    m_searchManager.moveToThread(m_searchThread);

    connect(this, &FilterManager::filterTracks, &m_searchManager, &TrackFilterer::filterTracks);
    connect(&m_searchManager, &TrackFilterer::tracksFiltered, this, &FilterManager::tracksFiltered);

    connect(m_library, &Core::Library::MusicLibrary::tracksLoaded, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::tracksUpdated, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::tracksAdded, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::tracksDeleted, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::tracksSorted, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::libraryChanged, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::libraryRemoved, this, &FilterManager::tracksChanged);

    connect(m_fieldsRegistry, &FieldRegistry::fieldChanged, this, &FilterManager::fieldChanged);

    m_searchThread->start();
}

FilterManager::~FilterManager()
{
    m_searchThread->quit();
    m_searchThread->wait();
}

Core::TrackList FilterManager::tracks() const
{
    return hasTracks() ? m_filteredTracks : m_library->tracks();
}

bool FilterManager::hasTracks() const
{
    return !m_filteredTracks.empty() || !m_searchFilter.isEmpty() || m_filterStore.hasActiveFilters();
}

LibraryFilter* FilterManager::registerFilter(const QString& name)
{
    const FilterField filterField = m_fieldsRegistry->fieldByName(name);
    return m_filterStore.addFilter(filterField);
}

void FilterManager::unregisterFilter(int index)
{
    m_filterStore.removeFilter(index);
    emit filteredItems(-1);
    getFilteredTracks();
}

void FilterManager::changeFilter(int index)
{
    m_filterStore.clearActiveFilters(index, true);
    getFilteredTracks();
    emit filteredItems(index - 1);
}

FilterField FilterManager::findField(const QString& name) const
{
    return m_fieldsRegistry->fieldByName(name);
}

void FilterManager::getFilteredTracks()
{
    m_filteredTracks.clear();

    for(auto& filter : m_filterStore.activeFilters()) {
        if(m_filteredTracks.empty()) {
            m_filteredTracks.insert(m_filteredTracks.cend(), filter.tracks.cbegin(), filter.tracks.cend());
        }
        else {
            m_filteredTracks
                = Utils::intersection<Core::Track, Core::Track::TrackHash>(filter.tracks, m_filteredTracks);
        }
    }
}

void FilterManager::selectionChanged(int index)
{
    m_filterStore.clearActiveFilters(index);
    getFilteredTracks();
    sendToPlaylist(m_playlistHandler, m_filteredTracks);
    emit filteredItems(index);
}

void FilterManager::searchChanged(const QString& search)
{
    const bool reset = m_searchFilter.length() > search.length();
    m_searchFilter   = search;
    emit filterTracks(!reset && !m_filteredTracks.empty() ? m_filteredTracks : m_library->tracks(), m_searchFilter);
}

QMenu* FilterManager::filterHeaderMenu(int index, FilterField* field)
{
    auto* menu = new QMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* filterList = new QActionGroup{menu};

    for(const auto& [filterIndex, registryField] : m_fieldsRegistry->fields()) {
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

void FilterManager::tracksFiltered(const Core::TrackList& tracks)
{
    m_filteredTracks = tracks;
    sendToPlaylist(m_playlistHandler, m_filteredTracks);
    emit filteredItems(-1);
}

void FilterManager::tracksChanged()
{
    getFilteredTracks();
    emit filteredItems(-1);
}
} // namespace Fy::Filters
