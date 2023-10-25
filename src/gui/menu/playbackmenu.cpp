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

#include "playbackmenu.h"

#include <core/player/playermanager.h>
#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QMenu>

namespace Fy::Gui {
PlaybackMenu::PlaybackMenu(Utils::ActionManager* actionManager, Core::Player::PlayerManager* playerManager,
                           QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_playerManager{playerManager}
    , m_playbackGroup{new QActionGroup(this)}
    , m_playIcon{QIcon::fromTheme(Gui::Constants::Icons::Play)}
    , m_pauseIcon{QIcon::fromTheme(Gui::Constants::Icons::Pause)}
{
    auto* playbackMenu = m_actionManager->actionContainer(Gui::Constants::Menus::Playback);

    const auto stopIcon = QIcon::fromTheme(Gui::Constants::Icons::Stop);
    const auto prevIcon = QIcon::fromTheme(Gui::Constants::Icons::Prev);
    const auto nextIcon = QIcon::fromTheme(Gui::Constants::Icons::Next);

    QObject::connect(m_playerManager, &Core::Player::PlayerManager::playStateChanged, this,
                     &PlaybackMenu::updatePlayPause);
    QObject::connect(m_playerManager, &Core::Player::PlayerManager::playModeChanged, this,
                     &PlaybackMenu::updatePlayMode);

    m_stop = new QAction(stopIcon, tr("&Stop"), this);
    actionManager->registerAction(m_stop, Gui::Constants::Actions::Stop);
    playbackMenu->addAction(m_stop, Gui::Constants::Groups::One);
    QObject::connect(m_stop, &QAction::triggered, playerManager, &Core::Player::PlayerManager::stop);

    m_playPause = new QAction(m_playIcon, tr("&Play"), this);
    m_actionManager->registerAction(m_playPause, Gui::Constants::Actions::PlayPause);
    playbackMenu->addAction(m_playPause, Gui::Constants::Groups::One);
    QObject::connect(m_playPause, &QAction::triggered, playerManager, &Core::Player::PlayerManager::playPause);

    m_next = new QAction(nextIcon, tr("&Next"), this);
    actionManager->registerAction(m_next, Gui::Constants::Actions::Next);
    playbackMenu->addAction(m_next, Gui::Constants::Groups::One);
    QObject::connect(m_next, &QAction::triggered, playerManager, &Core::Player::PlayerManager::next);

    m_previous = new QAction(prevIcon, tr("Pre&vious"), this);
    actionManager->registerAction(m_previous, Gui::Constants::Actions::Previous);
    playbackMenu->addAction(m_previous, Gui::Constants::Groups::One);
    QObject::connect(m_previous, &QAction::triggered, playerManager, &Core::Player::PlayerManager::previous);

    auto* orderMenu = m_actionManager->createMenu(Gui::Constants::Menus::PlaybackOrder);
    orderMenu->menu()->setTitle(tr("&Order"));
    orderMenu->appendGroup(Gui::Constants::Groups::One);
    playbackMenu->addMenu(orderMenu, Gui::Constants::Groups::Two);

    m_default = new QAction(tr("&Default"), this);
    m_default->setCheckable(true);
    actionManager->registerAction(m_default, Gui::Constants::Actions::PlaybackDefault);
    orderMenu->addAction(m_default, Gui::Constants::Groups::One);
    m_playbackGroup->addAction(m_default);
    QObject::connect(m_default, &QAction::triggered, this,
                     [this]() { m_playerManager->setPlayMode(Core::Player::PlayMode::Default); });

    m_repeat = new QAction(tr("&Repeat"), this);
    m_repeat->setCheckable(true);
    m_actionManager->registerAction(m_repeat, Gui::Constants::Actions::Repeat);
    orderMenu->addAction(m_repeat, Gui::Constants::Groups::One);
    m_playbackGroup->addAction(m_repeat);
    QObject::connect(m_repeat, &QAction::triggered, this,
                     [this]() { m_playerManager->setPlayMode(Core::Player::PlayMode::Repeat); });

    m_repeatAll = new QAction(tr("Repeat &All"), this);
    m_repeatAll->setCheckable(true);
    actionManager->registerAction(m_repeatAll, Gui::Constants::Actions::RepeatAll);
    orderMenu->addAction(m_repeatAll, Gui::Constants::Groups::One);
    m_playbackGroup->addAction(m_repeatAll);
    QObject::connect(m_repeatAll, &QAction::triggered, this,
                     [this]() { m_playerManager->setPlayMode(Core::Player::PlayMode::RepeatAll); });

    m_shuffle = new QAction(tr("&Shuffle"), this);
    m_shuffle->setCheckable(true);
    actionManager->registerAction(m_shuffle, Gui::Constants::Actions::Shuffle);
    orderMenu->addAction(m_shuffle, Gui::Constants::Groups::One);
    m_playbackGroup->addAction(m_shuffle);
    QObject::connect(m_shuffle, &QAction::triggered, this,
                     [this]() { m_playerManager->setPlayMode(Core::Player::PlayMode::Shuffle); });

    updatePlayPause(m_playerManager->playState());
    updatePlayMode(m_playerManager->playMode());
}

void PlaybackMenu::updatePlayPause(Core::Player::PlayState state)
{
    if(state == Core::Player::PlayState::Playing) {
        m_playPause->setText(tr("&Pause"));
        m_playPause->setIcon(m_pauseIcon);
    }
    else {
        m_playPause->setText(tr("&Play"));
        m_playPause->setIcon(m_playIcon);
    }
}

void PlaybackMenu::updatePlayMode(Core::Player::PlayMode mode)
{
    switch(mode) {
        case(Core::Player::PlayMode::Default): {
            m_default->setChecked(true);
            break;
        }
        case(Core::Player::PlayMode::Repeat): {
            m_repeat->setChecked(true);
            break;
        }
        case(Core::Player::PlayMode::RepeatAll): {
            m_repeatAll->setChecked(true);
            break;
        }
        case(Core::Player::PlayMode::Shuffle): {
            m_shuffle->setChecked(true);
            break;
        }
    }
}

} // namespace Fy::Gui
