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

#include <core/library/libraryinteractor.h>
#include <core/models/trackfwd.h>

class QMenu;

namespace Fy {
namespace Utils {
class ThreadManager;
}

namespace Core::Library {
class MusicLibrary;
} // namespace Core::Library

namespace Filters {
class FieldRegistry;

class FilterManager : public Core::Library::LibraryInteractor
{
    Q_OBJECT

public:
    explicit FilterManager(Utils::ThreadManager* threadManager, Core::Library::MusicLibrary* library,
                           FieldRegistry* fieldsRegistry, QObject* parent = nullptr);

    [[nodiscard]] Core::TrackPtrList tracks() const override;
    [[nodiscard]] bool hasTracks() const override;

    LibraryFilter* registerFilter(const QString& name);
    void unregisterFilter(int index);
    void changeFilter(int index);

    [[nodiscard]] FilterField findField(const QString& name) const;

    void getFilteredTracks();

    void selectionChanged(int index);
    void searchChanged(const QString& search);

    QMenu* filterHeaderMenu(int index, FilterField* field);

signals:
    void fieldChanged(const Fy::Filters::FilterField& field);
    void filterChanged(int index, const QString& name);
    void filterTracks(const Core::TrackPtrList& tracks, const QString& search);
    void filteredItems(int index);
    void filteredTracks();

private:
    void tracksFiltered(const Core::TrackPtrList& tracks);
    void tracksChanged();

    Utils::ThreadManager* m_threadManager;
    Core::Library::MusicLibrary* m_library;

    TrackFilterer m_searchManager;

    FieldRegistry* m_fieldsRegistry;
    Core::TrackPtrList m_filteredTracks;
    int m_lastFilterIndex;
    FilterStore m_filterStore;
    QString m_searchFilter;
};
} // namespace Filters
} // namespace Fy
