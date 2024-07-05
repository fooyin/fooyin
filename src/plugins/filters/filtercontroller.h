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

#include <core/track.h>
#include <utils/id.h>

#include <QObject>

namespace Fooyin {
class EditableLayout;
class MusicLibrary;
class SettingsManager;
class TagLoader;
class TrackSelectionController;

namespace Filters {
class FilterWidget;

struct FilterGroup
{
    Id id;
    std::vector<FilterWidget*> filters;
    TrackList filteredTracks;
    int updateCount{0};
};

using FilterGroups     = std::unordered_map<Id, FilterGroup, Id::IdHash>;
using UngroupedFilters = std::unordered_map<Id, FilterWidget*, Id::IdHash>;

class FilterController : public QObject
{
    Q_OBJECT

public:
    FilterController(MusicLibrary* library, TrackSelectionController* trackSelection, EditableLayout* editableLayout,
                     std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings, QObject* parent = nullptr);
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
    void tracksRemoved(const Fooyin::TrackList& tracks);
    void tracksUpdated(const Fooyin::TrackList& tracks);
    void tracksPlayed(const Fooyin::TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fooyin
