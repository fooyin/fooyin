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

#include "fygui_export.h"

#include <core/trackfwd.h>

#include <QObject>

class QMenu;

namespace Fooyin {
class ActionManager;
class SettingsManager;
class PlaylistController;

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

namespace PlaylistAction {
enum ActionOption
{
    None       = 1 << 0,
    Switch     = 1 << 1,
    KeepActive = 1 << 2,
};
Q_DECLARE_FLAGS(ActionOptions, ActionOption)
} // namespace PlaylistAction

class FYGUI_EXPORT TrackSelectionController : public QObject
{
    Q_OBJECT

public:
    TrackSelectionController(ActionManager* actionManager, SettingsManager* settings,
                             PlaylistController* playlistController, QObject* parent = nullptr);
    ~TrackSelectionController() override;

    [[nodiscard]] bool hasTracks() const;

    [[nodiscard]] TrackList selectedTracks() const;
    void changeSelectedTracks(int index, const TrackList& tracks, const QString& title = {});
    void changeSelectedTracks(const TrackList& tracks, const QString& title = {});

    void addTrackContextMenu(QMenu* menu) const;
    void addTrackPlaylistContextMenu(QMenu* menu) const;
    void executeAction(TrackAction action, PlaylistAction::ActionOptions options = {},
                       const QString& playlistName = {});

signals:
    void selectionChanged(const TrackList& tracks);
    void requestPropertiesDialog();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::PlaylistAction::ActionOptions)
