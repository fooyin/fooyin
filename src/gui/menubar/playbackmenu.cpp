/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QMenu>

namespace Fooyin {
struct PlaybackMenu::Private
{
    PlaybackMenu* self;

    ActionManager* actionManager;
    PlayerController* playerController;

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

    Private(PlaybackMenu* self_, ActionManager* actionManager_, PlayerController* playerController_)
        : self{self_}
        , actionManager{actionManager_}
        , playerController{playerController_}
        , playIcon{Utils::iconFromTheme(Constants::Icons::Play)}
        , pauseIcon{Utils::iconFromTheme(Constants::Icons::Pause)}
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
        repeat->setChecked(mode & Playlist::RepeatTrack);
        repeatAll->setChecked(mode & Playlist::RepeatPlaylist);
        shuffle->setChecked(mode & Playlist::ShuffleTracks);

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
        auto currentMode    = playerController->playMode();
        const bool noChange = currentMode == mode;

        if(mode == Playlist::Default) {
            currentMode = mode;
        }
        else if(mode & Playlist::RepeatTrack) {
            currentMode |= Playlist::RepeatTrack;
            currentMode &= ~Playlist::RepeatPlaylist;
        }
        else if(mode & Playlist::RepeatPlaylist) {
            currentMode |= Playlist::RepeatPlaylist;
            currentMode &= ~Playlist::RepeatTrack;
        }
        else {
            currentMode |= mode;
        }

        playerController->setPlayMode(currentMode);

        if(noChange) {
            updatePlayMode(playerController->playMode());
        }
    }
};

PlaybackMenu::PlaybackMenu(ActionManager* actionManager, PlayerController* playerController, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, actionManager, playerController)}
{
    auto* playbackMenu = p->actionManager->actionContainer(Constants::Menus::Playback);

    QObject::connect(p->playerController, &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->updatePlayPause(state); });
    QObject::connect(p->playerController, &PlayerController::playModeChanged, this,
                     [this](Playlist::PlayModes mode) { p->updatePlayMode(mode); });

    p->stop = new QAction(Utils::iconFromTheme(Constants::Icons::Stop), tr("&Stop"), this);
    playbackMenu->addAction(actionManager->registerAction(p->stop, Constants::Actions::Stop));
    QObject::connect(p->stop, &QAction::triggered, playerController, &PlayerController::stop);

    p->playPause = new QAction(p->playIcon, tr("&Play"), this);
    playbackMenu->addAction(actionManager->registerAction(p->playPause, Constants::Actions::PlayPause));
    QObject::connect(p->playPause, &QAction::triggered, playerController, &PlayerController::playPause);

    p->next = new QAction(Utils::iconFromTheme(Constants::Icons::Next), tr("&Next"), this);
    playbackMenu->addAction(actionManager->registerAction(p->next, Constants::Actions::Next));
    QObject::connect(p->next, &QAction::triggered, playerController, &PlayerController::next);

    p->previous = new QAction(Utils::iconFromTheme(Constants::Icons::Prev), tr("Pre&vious"), this);
    playbackMenu->addAction(actionManager->registerAction(p->previous, Constants::Actions::Previous));
    QObject::connect(p->previous, &QAction::triggered, playerController, &PlayerController::previous);

    auto* orderMenu = p->actionManager->createMenu(Constants::Menus::PlaybackOrder);
    orderMenu->menu()->setTitle(tr("&Order"));
    playbackMenu->addMenu(orderMenu, Actions::Groups::Three);

    p->defaultPlayback = new QAction(tr("&Default"), this);
    p->defaultPlayback->setCheckable(true);
    orderMenu->addAction(actionManager->registerAction(p->defaultPlayback, Constants::Actions::PlaybackDefault));
    QObject::connect(p->defaultPlayback, &QAction::triggered, this, [this]() { p->setPlayMode(Playlist::Default); });

    p->repeat = new QAction(tr("&Repeat Track"), this);
    p->repeat->setCheckable(true);
    orderMenu->addAction(p->actionManager->registerAction(p->repeat, Constants::Actions::RepeatTrack));
    QObject::connect(p->repeat, &QAction::triggered, this,
                     [this]() { p->setPlayMode(Playlist::PlayMode::RepeatTrack); });

    p->repeatAll = new QAction(tr("Repeat &Playlist"), this);
    p->repeatAll->setCheckable(true);
    orderMenu->addAction(actionManager->registerAction(p->repeatAll, Constants::Actions::RepeatPlaylist));
    QObject::connect(p->repeatAll, &QAction::triggered, this,
                     [this]() { p->setPlayMode(Playlist::PlayMode::RepeatPlaylist); });

    p->shuffle = new QAction(tr("&Shuffle Tracks"), this);
    p->shuffle->setCheckable(true);
    orderMenu->addAction(actionManager->registerAction(p->shuffle, Constants::Actions::ShuffleTracks));
    QObject::connect(p->shuffle, &QAction::triggered, this,
                     [this]() { p->setPlayMode(Playlist::PlayMode::ShuffleTracks); });

    p->updatePlayPause(p->playerController->playState());
    p->updatePlayMode(p->playerController->playMode());
}

PlaybackMenu::~PlaybackMenu() = default;
} // namespace Fooyin

#include "moc_playbackmenu.cpp"
