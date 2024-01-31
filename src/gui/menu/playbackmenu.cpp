/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
struct PlaybackMenu::Private
{
    PlaybackMenu* self;

    ActionManager* actionManager;
    PlayerManager* playerManager;

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

    Private(PlaybackMenu* self_, ActionManager* actionManager_, PlayerManager* playerManager_)
        : self{self_}
        , actionManager{actionManager_}
        , playerManager{playerManager_}
        , playIcon{QIcon::fromTheme(Constants::Icons::Play)}
        , pauseIcon{QIcon::fromTheme(Constants::Icons::Pause)}
    { }

    void updatePlayPause(PlayState state) const
    {
        if(state == PlayState::Playing) {
            playPause->setText(tr("&Pause"));
            playPause->setIcon(pauseIcon);
        }
        else {
            playPause->setText(tr("&Play"));
            playPause->setIcon(playIcon);
        }
    }

    void updatePlayMode(Playlist::PlayModes mode) const
    {
        repeat->setChecked(mode & Playlist::Repeat);
        repeatAll->setChecked(mode & Playlist::RepeatAll);
        shuffle->setChecked(mode & Playlist::Shuffle);

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

    void setPlayMode(Playlist::PlayMode mode) const
    {
        auto currentMode    = playerManager->playMode();
        const bool noChange = currentMode == mode;

        if(mode == Playlist::Default) {
            currentMode = mode;
        }
        else if(mode & Playlist::Repeat) {
            currentMode |= Playlist::Repeat;
            currentMode &= ~Playlist::RepeatAll;
        }
        else if(mode & Playlist::RepeatAll) {
            currentMode |= Playlist::RepeatAll;
            currentMode &= ~Playlist::Repeat;
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

PlaybackMenu::PlaybackMenu(ActionManager* actionManager, PlayerManager* playerManager, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, actionManager, playerManager)}
{
    auto* playbackMenu = p->actionManager->actionContainer(Constants::Menus::Playback);

    const auto stopIcon = QIcon::fromTheme(Constants::Icons::Stop);
    const auto prevIcon = QIcon::fromTheme(Constants::Icons::Prev);
    const auto nextIcon = QIcon::fromTheme(Constants::Icons::Next);

    QObject::connect(p->playerManager, &PlayerManager::playStateChanged, this,
                     [this](PlayState state) { p->updatePlayPause(state); });
    QObject::connect(p->playerManager, &PlayerManager::playModeChanged, this,
                     [this](Playlist::PlayModes mode) { p->updatePlayMode(mode); });

    p->stop = new QAction(stopIcon, tr("&Stop"), this);
    playbackMenu->addAction(actionManager->registerAction(p->stop, Constants::Actions::Stop));
    QObject::connect(p->stop, &QAction::triggered, playerManager, &PlayerManager::stop);

    p->playPause = new QAction(p->playIcon, tr("&Play"), this);
    playbackMenu->addAction(actionManager->registerAction(p->playPause, Constants::Actions::PlayPause));
    QObject::connect(p->playPause, &QAction::triggered, playerManager, &PlayerManager::playPause);

    p->next = new QAction(nextIcon, tr("&Next"), this);
    playbackMenu->addAction(actionManager->registerAction(p->next, Constants::Actions::Next));
    QObject::connect(p->next, &QAction::triggered, playerManager, &PlayerManager::next);

    p->previous = new QAction(prevIcon, tr("Pre&vious"), this);
    playbackMenu->addAction(actionManager->registerAction(p->previous, Constants::Actions::Previous));
    QObject::connect(p->previous, &QAction::triggered, playerManager, &PlayerManager::previous);

    auto* orderMenu = p->actionManager->createMenu(Constants::Menus::PlaybackOrder);
    orderMenu->menu()->setTitle(tr("&Order"));
    playbackMenu->addMenu(orderMenu, Actions::Groups::Three);

    p->defaultPlayback = new QAction(tr("&Default"), this);
    p->defaultPlayback->setCheckable(true);
    orderMenu->addAction(actionManager->registerAction(p->defaultPlayback, Constants::Actions::PlaybackDefault));
    QObject::connect(p->defaultPlayback, &QAction::triggered, this, [this]() { p->setPlayMode(Playlist::Default); });

    p->repeat = new QAction(tr("&Repeat"), this);
    p->repeat->setCheckable(true);
    orderMenu->addAction(p->actionManager->registerAction(p->repeat, Constants::Actions::Repeat));
    QObject::connect(p->repeat, &QAction::triggered, this, [this]() { p->setPlayMode(Playlist::PlayMode::Repeat); });

    p->repeatAll = new QAction(tr("Repeat &All"), this);
    p->repeatAll->setCheckable(true);
    orderMenu->addAction(actionManager->registerAction(p->repeatAll, Constants::Actions::RepeatAll));
    QObject::connect(p->repeatAll, &QAction::triggered, this,
                     [this]() { p->setPlayMode(Playlist::PlayMode::RepeatAll); });

    p->shuffle = new QAction(tr("&Shuffle"), this);
    p->shuffle->setCheckable(true);
    orderMenu->addAction(actionManager->registerAction(p->shuffle, Constants::Actions::Shuffle));
    QObject::connect(p->shuffle, &QAction::triggered, this, [this]() { p->setPlayMode(Playlist::PlayMode::Shuffle); });

    p->updatePlayPause(p->playerManager->playState());
    p->updatePlayMode(p->playerManager->playMode());
}

PlaybackMenu::~PlaybackMenu() = default;
} // namespace Fooyin

#include "moc_playbackmenu.cpp"
