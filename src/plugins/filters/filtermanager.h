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

#include <core/trackfwd.h>

#include <QObject>

#include <QCoroTask>

namespace Fooyin {
class SettingsManager;
class MusicLibrary;
class TrackSelectionController;

namespace Filters {
class FilterColumnRegistry;
class FilterWidget;

class FilterManager : public QObject
{
    Q_OBJECT

public:
    explicit FilterManager(MusicLibrary* library, TrackSelectionController* trackSelection, SettingsManager* settings,
                           QObject* parent = nullptr);
    ~FilterManager() override;

    void shutdown();

    FilterWidget* createFilter();

    [[nodiscard]] FilterColumnRegistry* columnRegistry() const;

signals:
    void tracksRemoved(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);

public slots:
    QCoro::Task<void> searchChanged(QString search);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fooyin
