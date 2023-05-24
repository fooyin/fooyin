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

#include "playercontrol.h"

#include "gui/guiconstants.h"

#include <core/player/playermanager.h>

#include <utils/comboicon.h>

#include <QHBoxLayout>

namespace Fy::Gui::Widgets {
using Utils::ComboIcon;

PlayerControl::PlayerControl(Core::Player::PlayerManager* playerManager, QWidget* parent)
    : QWidget{parent}
    , m_playerManager{playerManager}
    , m_layout{new QHBoxLayout(this)}
    , m_stop{new ComboIcon(Constants::Icons::Stop, ComboIcon::HasDisabledIcon, this)}
    , m_prev{new ComboIcon(Constants::Icons::Prev, ComboIcon::HasDisabledIcon, this)}
    , m_playPause{new ComboIcon(Constants::Icons::Play, ComboIcon::HasDisabledIcon, this)}
    , m_next{new ComboIcon(Constants::Icons::Next, ComboIcon::HasDisabledIcon, this)}
    , m_labelSize{20, 20}
{
    setupUi();

    connect(m_stop, &ComboIcon::clicked, m_playerManager, &Core::Player::PlayerManager::stop);
    connect(m_prev, &ComboIcon::clicked, m_playerManager, &Core::Player::PlayerManager::previous);
    connect(m_playPause, &ComboIcon::clicked, m_playerManager, &Core::Player::PlayerManager::playPause);
    connect(m_next, &ComboIcon::clicked, m_playerManager, &Core::Player::PlayerManager::next);

    connect(m_playerManager, &Core::Player::PlayerManager::playStateChanged, this, &PlayerControl::stateChanged);
}

void PlayerControl::setupUi()
{
    m_layout->setSizeConstraint(QLayout::SetFixedSize);
    m_layout->setSpacing(10);
    m_layout->setContentsMargins(10, 0, 0, 0);

    m_playPause->addIcon(Constants::Icons::Pause);

    m_stop->setMaximumSize(m_labelSize);
    m_prev->setMaximumSize(m_labelSize);
    m_playPause->setMaximumSize(m_labelSize);
    m_next->setMaximumSize(m_labelSize);

    m_layout->addWidget(m_stop, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_prev, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_playPause, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_next, 0, Qt::AlignVCenter);

    stateChanged(m_playerManager->playState());
}

void PlayerControl::stateChanged(Core::Player::PlayState state)
{
    switch(state) {
        case(Core::Player::Stopped):
            m_playPause->setIcon(Constants::Icons::Play);
            return setEnabled(false);
        case(Core::Player::Playing):
            m_playPause->setIcon(Constants::Icons::Pause);
            return setEnabled(true);
        case(Core::Player::Paused):
            return m_playPause->setIcon(Constants::Icons::Play);
    }
}
} // namespace Fy::Gui::Widgets
