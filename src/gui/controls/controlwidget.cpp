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
    , m_playControls{new PlayerControl(this)}
    , m_playlistControls{new PlaylistControl(m_settings, this)}
    , m_volumeControls{new VolumeControl(this)}
    , m_progress{new ProgressWidget(m_settings, this)}
{
    setObjectName("Control Bar");

    setupUi();
    setupConnections();
}

void ControlWidget::setupUi()
{
    //    setEnabled(false);

    m_layout->addWidget(m_playControls, 0, Qt::AlignLeft | Qt::AlignVCenter);
    m_layout->addWidget(m_progress, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_playlistControls, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_volumeControls, 0, Qt::AlignVCenter);

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(15);
}

void ControlWidget::setupConnections()
{
    connect(m_playerManager, &Core::Player::PlayerManager::currentTrackChanged, this,
            &ControlWidget::currentTrackChanged);
    connect(m_playerManager, &Core::Player::PlayerManager::positionChanged, this,
            &ControlWidget::currentPositionChanged);
    connect(m_playerManager, &Core::Player::PlayerManager::playStateChanged, m_playControls,
            &PlayerControl::stateChanged);
    connect(m_playerManager, &Core::Player::PlayerManager::playStateChanged, m_progress, &ProgressWidget::stateChanged);

    connect(m_playControls, &PlayerControl::stopClicked, m_playerManager, &Core::Player::PlayerManager::stop);
    connect(m_playControls, &PlayerControl::nextClicked, m_playerManager, &Core::Player::PlayerManager::next);
    connect(m_playControls, &PlayerControl::prevClicked, m_playerManager, &Core::Player::PlayerManager::previous);
    connect(m_playControls, &PlayerControl::stopClicked, m_progress, &ProgressWidget::reset);
    connect(m_playControls, &PlayerControl::pauseClicked, m_playerManager, &Core::Player::PlayerManager::playPause);

    connect(m_volumeControls, &VolumeControl::volumeUp, m_playerManager, &Core::Player::PlayerManager::volumeUp);
    connect(m_volumeControls, &VolumeControl::volumeDown, m_playerManager, &Core::Player::PlayerManager::volumeDown);
    connect(m_volumeControls, &VolumeControl::volumeChanged, m_playerManager, &Core::Player::PlayerManager::setVolume);

    connect(m_progress, &ProgressWidget::movedSlider, m_playerManager, &Core::Player::PlayerManager::changePosition);

    connect(m_playlistControls, &PlaylistControl::repeatClicked, m_playerManager,
            &Core::Player::PlayerManager::setRepeat);
    connect(m_playlistControls, &PlaylistControl::shuffleClicked, m_playerManager,
            &Core::Player::PlayerManager::setShuffle);

    connect(m_playerManager, &Core::Player::PlayerManager::playModeChanged, m_playlistControls,
            &PlaylistControl::playModeChanged);
}

QString ControlWidget::name() const
{
    return "Controls";
}

void ControlWidget::contextMenuEvent(QContextMenuEvent* event)
{
    Q_UNUSED(event)
}

void ControlWidget::currentTrackChanged(Core::Track* track)
{
    //    if (!isEnabled())
    //    {
    //        setEnabled(true);
    //    }
    m_progress->changeTrack(track);
}

void ControlWidget::currentPositionChanged(qint64 ms)
{
    m_progress->setCurrentPosition(static_cast<int>(ms));
}
} // namespace Gui::Widgets
