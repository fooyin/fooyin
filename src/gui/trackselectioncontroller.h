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

class QMenu;

namespace Fy {
namespace Utils {
class ActionManager;
class SettingsManager;
} // namespace Utils

namespace Gui {
namespace Widgets::Playlist {
class PlaylistController;
}

enum class TrackAction
{
    None,
    Expand,
    AddCurrentPlaylist,
    AddActivePlaylist,
    SendCurrentPlaylist,
    SendNewPlaylist,
    Play
};

enum ActionOption
{
    None       = 1 << 0,
    Switch     = 1 << 1,
    KeepActive = 1 << 2,
};
Q_DECLARE_FLAGS(ActionOptions, ActionOption)

class TrackSelectionController : public QObject
{
    Q_OBJECT

public:
    TrackSelectionController(Utils::ActionManager* actionManager, Utils::SettingsManager* settings,
                             Widgets::Playlist::PlaylistController* playlistController);
    ~TrackSelectionController() override;

    [[nodiscard]] bool hasTracks() const;

    [[nodiscard]] Core::TrackList selectedTracks() const;
    void changeSelectedTracks(const Core::TrackList& tracks, const QString& title = {});

    void addTrackContextMenu(QMenu* menu) const;
    void addTrackPlaylistContextMenu(QMenu* menu) const;
    void executeAction(TrackAction action, ActionOptions options = {}, const QString& playlistName = {});

signals:
    void selectionChanged(const Core::TrackList& tracks);
    void requestPropertiesDialog();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Gui
} // namespace Fy

Q_DECLARE_OPERATORS_FOR_FLAGS(Fy::Gui::ActionOptions)
