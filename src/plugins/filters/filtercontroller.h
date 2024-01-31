/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/trackfwd.h>
#include <utils/id.h>

#include <QObject>

namespace Fooyin {
class EditableLayout;
class SettingsManager;
class MusicLibrary;
class TrackSelectionController;

namespace Filters {
class FilterWidget;

struct FilterGroup
{
    Id id;
    std::vector<FilterWidget*> filters;
    TrackList filteredTracks;
};

using FilterGroups     = std::unordered_map<Id, FilterGroup, Id::IdHash>;
using UngroupedFilters = std::unordered_map<Id, FilterWidget*, Id::IdHash>;

class FilterController : public QObject
{
    Q_OBJECT

public:
    FilterController(MusicLibrary* library, TrackSelectionController* trackSelection, EditableLayout* editableLayout,
                     SettingsManager* settings, QObject* parent = nullptr);
    ~FilterController() override;

    FilterWidget* createFilter();

    bool haveUngroupedFilters() const;
    bool filterIsUngrouped(const Id& id) const;

    FilterGroups filterGroups() const;
    std::optional<FilterGroup> groupById(const Id& id) const;
    UngroupedFilters ungroupedFilters() const;

    void addFilterToGroup(FilterWidget* widget, const Id& groupId);
    bool removeFilter(FilterWidget* widget);

signals:
    void tracksRemoved(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fooyin
