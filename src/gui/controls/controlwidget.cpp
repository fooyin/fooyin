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

#include "controlwidget.h"

#include "playercontrol.h"
#include "playlistcontrol.h"
#include "progresswidget.h"
#include "volumecontrol.h"

#include <core/models/track.h>
#include <core/player/playermanager.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>

namespace Gui::Widgets {
ControlWidget::ControlWidget(Core::Player::PlayerManager* playerManager, Core::SettingsManager* settings,
                             QWidget* parent)
    : FyWidget{parent}
    , m_playerManager{playerManager}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(this)}
    , m_playerControls{new PlayerControl(m_playerManager, this)}
    , m_playlistControls{new PlaylistControl(m_playerManager, m_settings, this)}
    , m_volumeControls{new VolumeControl(m_playerManager, this)}
    , m_progress{new ProgressWidget(m_playerManager, m_settings, this)}
{
    setObjectName("Control Bar");

    setupUi();
}

void ControlWidget::setupUi()
{
    m_layout->addWidget(m_playerControls, 0, Qt::AlignLeft | Qt::AlignVCenter);
    m_layout->addWidget(m_progress, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_playlistControls, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_volumeControls, 0, Qt::AlignVCenter);

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(15);

    m_playerControls->setEnabled(m_playerManager->currentTrack());
    m_progress->setEnabled(m_playerManager->currentTrack());
    m_progress->changeTrack(m_playerManager->currentTrack());
}

QString ControlWidget::name() const
{
    return "Controls";
}
} // namespace Gui::Widgets
