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

#include <core/models/trackfwd.h>

#include <QObject>

#include <QCoroTask>

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Core {
namespace Playlist {
class PlaylistHandler;
}
namespace Library {
class MusicLibrary;
}
} // namespace Core

namespace Gui {
class TrackSelectionController;
} // namespace Gui

namespace Filters {
class FieldRegistry;
class FilterWidget;

class FilterManager : public QObject
{
    Q_OBJECT

public:
    explicit FilterManager(Core::Library::MusicLibrary* library, Core::Playlist::PlaylistHandler* playlistHandler,
                           Gui::TrackSelectionController* trackSelection, Utils::SettingsManager* settings,
                           QObject* parent = nullptr);
    ~FilterManager() override;

    void shutdown();

    FilterWidget* createFilter();

    [[nodiscard]] FieldRegistry* fieldRegistry() const;

signals:
    void tracksAdded(const Core::TrackList& tracks);
    void tracksRemoved(const Core::TrackList& tracks);
    void tracksUpdated(const Core::TrackList& tracks);

public slots:
    QCoro::Task<void> searchChanged(QString search);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fy
