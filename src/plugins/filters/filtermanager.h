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

#include "filterstore.h"
#include "trackfilterer.h"

#include <core/models/trackfwd.h>

class QMenu;

namespace Fy {
namespace Core {
namespace Playlist {
class PlaylistHandler;
}
namespace Library {
class MusicLibrary;
}
} // namespace Core

namespace Filters {
class FieldRegistry;

class FilterManager : public QObject
{
    Q_OBJECT

public:
    explicit FilterManager(Core::Library::MusicLibrary* library, Core::Playlist::PlaylistHandler* playlistHandler,
                           FieldRegistry* fieldsRegistry, QObject* parent = nullptr);
    ~FilterManager() override;

    [[nodiscard]] Core::TrackList tracks() const;
    [[nodiscard]] bool hasTracks() const;

    LibraryFilter* registerFilter(const QString& name);
    void unregisterFilter(int index);
    void changeFilter(int index);

    [[nodiscard]] FilterField findField(const QString& name) const;

    void getFilteredTracks();

    void selectionChanged(int index);
    void searchChanged(const QString& search);

    QMenu* filterHeaderMenu(int index, FilterField* field);

signals:
    void fieldChanged(const Filters::FilterField& field);
    void filterChanged(int index, const QString& name);
    void filterTracks(const Core::TrackList& tracks, const QString& search);
    void filteredItems(int index);

private:
    void tracksFiltered(const Core::TrackList& tracks);
    void tracksChanged();

    Core::Library::MusicLibrary* m_library;
    Core::Playlist::PlaylistHandler* m_playlistHandler;

    QThread* m_searchThread;
    TrackFilterer m_searchManager;

    FieldRegistry* m_fieldsRegistry;
    Core::TrackList m_filteredTracks;
    FilterStore m_filterStore;
    QString m_searchFilter;
};
} // namespace Filters
} // namespace Fy
