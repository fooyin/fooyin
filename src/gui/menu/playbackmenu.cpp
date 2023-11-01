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
struct PlaybackMenu::Private
{
    PlaybackMenu* self;

    Utils::ActionManager* actionManager;
    Core::Player::PlayerManager* playerManager;

    QAction* stop{nullptr};
    QAction* playPause{nullptr};
    QAction* previous{nullptr};
    QAction* next{nullptr};

    QAction* defaultPlayback{nullptr};
    QAction* repeat{nullptr};
    QAction* repeatAll{nullptr};
    QAction* shuffle{nullptr};

    QIcon playIcon;
    QIcon pauseIcon;

    Private(PlaybackMenu* self, Utils::ActionManager* actionManager, Core::Player::PlayerManager* playerManager)
        : self{self}
        , actionManager{actionManager}
        , playerManager{playerManager}
        , playIcon{QIcon::fromTheme(Gui::Constants::Icons::Play)}
        , pauseIcon{QIcon::fromTheme(Gui::Constants::Icons::Pause)}
    { }

    void updatePlayPause(Core::Player::PlayState state) const
    {
        if(state == Core::Player::PlayState::Playing) {
            playPause->setText(tr("&Pause"));
            playPause->setIcon(pauseIcon);
        }
        else {
            playPause->setText(tr("&Play"));
            playPause->setIcon(playIcon);
        }
    }

    void updatePlayMode(Core::Playlist::PlayModes mode) const
    {
        repeat->setChecked(mode & Core::Playlist::Repeat);
        repeatAll->setChecked(mode & Core::Playlist::RepeatAll);
        shuffle->setChecked(mode & Core::Playlist::Shuffle);

        if(mode == 0) {
            defaultPlayback->setChecked(true);
            repeat->setChecked(false);
            repeatAll->setChecked(false);
            shuffle->setChecked(false);
        }
        else {
            defaultPlayback->setChecked(false);
        }
    }

    void setPlayMode(Core::Playlist::PlayMode mode) const
    {
        auto currentMode    = playerManager->playMode();
        const bool noChange = currentMode == mode;

        if(mode == Core::Playlist::Default) {
            currentMode = mode;
        }
        else if(mode & Core::Playlist::Repeat) {
            currentMode |= Core::Playlist::Repeat;
            currentMode &= ~Core::Playlist::RepeatAll;
        }
        else if(mode & Core::Playlist::RepeatAll) {
            currentMode |= Core::Playlist::RepeatAll;
            currentMode &= ~Core::Playlist::Repeat;
        }
        else {
            currentMode |= mode;
        }
        playerManager->setPlayMode(currentMode);

        if(noChange) {
            updatePlayMode(playerManager->playMode());
        }
    }
};

PlaybackMenu::PlaybackMenu(Utils::ActionManager* actionManager, Core::Player::PlayerManager* playerManager,
                           QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, actionManager, playerManager)}
{
    auto* playbackMenu = p->actionManager->actionContainer(Gui::Constants::Menus::Playback);

    const auto stopIcon = QIcon::fromTheme(Gui::Constants::Icons::Stop);
    const auto prevIcon = QIcon::fromTheme(Gui::Constants::Icons::Prev);
    const auto nextIcon = QIcon::fromTheme(Gui::Constants::Icons::Next);

    QObject::connect(p->playerManager, &Core::Player::PlayerManager::playStateChanged, this,
                     [this](Core::Player::PlayState state) { p->updatePlayPause(state); });
    QObject::connect(p->playerManager, &Core::Player::PlayerManager::playModeChanged, this,
                     [this](Core::Playlist::PlayModes mode) { p->updatePlayMode(mode); });

    p->stop = new QAction(stopIcon, tr("&Stop"), this);
    playbackMenu->addAction(actionManager->registerAction(p->stop, Gui::Constants::Actions::Stop));
    QObject::connect(p->stop, &QAction::triggered, playerManager, &Core::Player::PlayerManager::stop);

    p->playPause = new QAction(p->playIcon, tr("&Play"), this);
    playbackMenu->addAction(actionManager->registerAction(p->playPause, Gui::Constants::Actions::PlayPause));
    QObject::connect(p->playPause, &QAction::triggered, playerManager, &Core::Player::PlayerManager::playPause);

    p->next = new QAction(nextIcon, tr("&Next"), this);
    playbackMenu->addAction(actionManager->registerAction(p->next, Gui::Constants::Actions::Next));
    QObject::connect(p->next, &QAction::triggered, playerManager, &Core::Player::PlayerManager::next);

    p->previous = new QAction(prevIcon, tr("Pre&vious"), this);
    playbackMenu->addAction(actionManager->registerAction(p->previous, Gui::Constants::Actions::Previous));
    QObject::connect(p->previous, &QAction::triggered, playerManager, &Core::Player::PlayerManager::previous);

    auto* orderMenu = p->actionManager->createMenu(Gui::Constants::Menus::PlaybackOrder);
    orderMenu->menu()->setTitle(tr("&Order"));
    playbackMenu->addMenu(orderMenu, Utils::Actions::Groups::Three);

    p->defaultPlayback = new QAction(tr("&Default"), this);
    p->defaultPlayback->setCheckable(true);
    orderMenu->addAction(actionManager->registerAction(p->defaultPlayback, Gui::Constants::Actions::PlaybackDefault));
    QObject::connect(p->defaultPlayback, &QAction::triggered, this,
                     [this]() { p->setPlayMode(Core::Playlist::Default); });

    p->repeat = new QAction(tr("&Repeat"), this);
    p->repeat->setCheckable(true);
    orderMenu->addAction(p->actionManager->registerAction(p->repeat, Gui::Constants::Actions::Repeat));
    QObject::connect(p->repeat, &QAction::triggered, this,
                     [this]() { p->setPlayMode(Core::Playlist::PlayMode::Repeat); });

    p->repeatAll = new QAction(tr("Repeat &All"), this);
    p->repeatAll->setCheckable(true);
    orderMenu->addAction(actionManager->registerAction(p->repeatAll, Gui::Constants::Actions::RepeatAll));
    QObject::connect(p->repeatAll, &QAction::triggered, this,
                     [this]() { p->setPlayMode(Core::Playlist::PlayMode::RepeatAll); });

    p->shuffle = new QAction(tr("&Shuffle"), this);
    p->shuffle->setCheckable(true);
    orderMenu->addAction(actionManager->registerAction(p->shuffle, Gui::Constants::Actions::Shuffle));
    QObject::connect(p->shuffle, &QAction::triggered, this,
                     [this]() { p->setPlayMode(Core::Playlist::PlayMode::Shuffle); });

    p->updatePlayPause(p->playerManager->playState());
    p->updatePlayMode(p->playerManager->playMode());
}

PlaybackMenu::~PlaybackMenu() = default;
} // namespace Fy::Gui
