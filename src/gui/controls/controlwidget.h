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

#include <QWidget>

class QHBoxLayout;

namespace Core {
class Track;
class SettingsManager;

namespace Player {
class PlayerManager;
}
} // namespace Core

namespace Gui::Widgets {
class PlayerControl;
class PlaylistControl;
class VolumeControl;
class ProgressWidget;

class ControlWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit ControlWidget(Core::Player::PlayerManager* playerManager, Core::SettingsManager* settings,
                           QWidget* parent = nullptr);
    ~ControlWidget() override;

    void setupUi();
    void setupConnections();

    [[nodiscard]] QString name() const override;

signals:
    void stop();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

    void currentTrackChanged(Core::Track* track);
    void currentPositionChanged(qint64 ms);

private:
    Core::Player::PlayerManager* m_playerManager;
    Core::SettingsManager* m_settings;

    QHBoxLayout* m_layout;
    PlayerControl* m_playControls;
    PlaylistControl* m_playlistControls;
    VolumeControl* m_volumeControls;
    ProgressWidget* m_progress;
};
} // namespace Gui::Widgets
