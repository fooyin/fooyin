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

#include "gui/fywidget.h"

#include <core/models/trackfwd.h>

namespace Fy {

namespace Utils {
class SettingsManager;
} // namespace Utils

namespace Core {
namespace Player {
class PlayerManager;
} // namespace Player

namespace Library {
class MusicLibrary;
} // namespace Library
} // namespace Core

namespace Gui::Widgets::Playlist {
class PlaylistController;
class PresetRegistry;

class PlaylistWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
                            PlaylistController* playlistController, PresetRegistry* presetRegistry,
                            Utils::SettingsManager* settings, QWidget* parent = nullptr);
    ~PlaylistWidget() override;

    [[nodiscard]] QString name() const override;

signals:
    void selectionWasChanged(const Core::TrackList& tracks);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Gui::Widgets::Playlist
} // namespace Fy
