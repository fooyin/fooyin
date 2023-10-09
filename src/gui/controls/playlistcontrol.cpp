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

#include "playlistcontrol.h"

#include <core/coresettings.h>
#include <core/player/playermanager.h>
#include <gui/guiconstants.h>
#include <utils/comboicon.h>
#include <utils/enumhelper.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>

namespace Fy::Gui::Widgets {
PlaylistControl::PlaylistControl(Core::Player::PlayerManager* playerManager, QWidget* parent)
    : QWidget{parent}
    , m_playerManager{playerManager}
    , m_layout{new QHBoxLayout(this)}
    , m_labelSize{20, 20}
    , m_repeat{new Utils::ComboIcon(Constants::Icons::RepeatAll, Utils::ComboIcon::HasActiveIcon, this)}
    , m_shuffle{new Utils::ComboIcon(Constants::Icons::Shuffle, Utils::ComboIcon::HasActiveIcon, this)}

{
    setupUi();

    connect(m_repeat, &Utils::ComboIcon::clicked, this, &PlaylistControl::repeatClicked);
    connect(m_shuffle, &Utils::ComboIcon::clicked, this, &PlaylistControl::shuffleClicked);

    connect(m_playerManager, &Core::Player::PlayerManager::playModeChanged, this, &PlaylistControl::setMode);

    setMode(m_playerManager->playMode());
}

void PlaylistControl::setupUi()
{
    m_layout->setSizeConstraint(QLayout::SetFixedSize);
    m_layout->setSpacing(10);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_repeat->addIcon(Constants::Icons::Repeat);

    m_repeat->setMaximumSize(m_labelSize);
    m_shuffle->setMaximumSize(m_labelSize);

    m_layout->addWidget(m_repeat, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_shuffle, 0, Qt::AlignVCenter);
}

void PlaylistControl::repeatClicked()
{
    const auto mode = m_playerManager->playMode();

    switch(mode) {
        case(Core::Player::PlayMode::Repeat):
            m_playerManager->setPlayMode(Core::Player::PlayMode::Default);
            break;
        case(Core::Player::PlayMode::RepeatAll):
            m_playerManager->setPlayMode(Core::Player::PlayMode::Repeat);
            break;
        case(Core::Player::PlayMode::Shuffle):
        case(Core::Player::PlayMode::Default):
            m_playerManager->setPlayMode(Core::Player::PlayMode::RepeatAll);
            break;
    }
}

void PlaylistControl::shuffleClicked()
{
    const auto mode = m_playerManager->playMode();

    if(mode == Core::Player::PlayMode::Shuffle) {
        m_playerManager->setPlayMode(Core::Player::PlayMode::Default);
    }
    else {
        m_playerManager->setPlayMode(Core::Player::PlayMode::Shuffle);
    }
}

void PlaylistControl::setMode(Core::Player::PlayMode mode) const
{
    switch(mode) {
        case(Core::Player::PlayMode::Repeat):
            m_repeat->setIcon(Constants::Icons::Repeat, true);
            m_shuffle->setIcon(Constants::Icons::Shuffle);
            break;
        case(Core::Player::PlayMode::RepeatAll):
            m_repeat->setIcon(Constants::Icons::RepeatAll, true);
            m_shuffle->setIcon(Constants::Icons::Shuffle);
            break;
        case(Core::Player::PlayMode::Shuffle):
            m_shuffle->setIcon(Constants::Icons::Shuffle, true);
            m_repeat->setIcon(Constants::Icons::RepeatAll);
            break;
        case(Core::Player::PlayMode::Default):
            m_repeat->setIcon(Constants::Icons::RepeatAll);
            m_shuffle->setIcon(Constants::Icons::Shuffle);
            break;
    }
}
} // namespace Fy::Gui::Widgets
