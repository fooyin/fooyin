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

#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QActionGroup>
#include <QMenu>

namespace Fooyin {
PlaybackMenu::PlaybackMenu(ActionManager* actionManager, PlayerController* playerController, SettingsManager* settings,
                           QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_playIcon{Utils::iconFromTheme(Constants::Icons::Play)}
    , m_pauseIcon{Utils::iconFromTheme(Constants::Icons::Pause)}
    , m_stop{new QAction(Utils::iconFromTheme(Constants::Icons::Stop), tr("Stop"), this)}
    , m_playPause{new QAction(m_playIcon, tr("&Play"), this)}
    , m_previous{new QAction(Utils::iconFromTheme(Constants::Icons::Prev), tr("Previous"), this)}
    , m_next{new QAction(Utils::iconFromTheme(Constants::Icons::Next), tr("Next"), this)}
    , m_defaultPlayback{new QAction(tr("&Default"), this)}
    , m_repeatTrack{new QAction(tr("&Repeat track"), this)}
    , m_repeatPlaylist{new QAction(tr("Repeat &playlist"), this)}
    , m_shuffle{new QAction(tr("&Shuffle tracks"), this)}
    , m_stopAfterCurrent{new QAction(tr("Stop &after current"), this)}
{
    auto* playbackMenu = m_actionManager->actionContainer(Constants::Menus::Playback);

    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     [this](Player::PlayState state) { updatePlayPause(state); });
    QObject::connect(m_playerController, &PlayerController::playModeChanged, this,
                     [this](Playlist::PlayModes mode) { updatePlayMode(mode); });

    playbackMenu->addAction(actionManager->registerAction(m_stop, Constants::Actions::Stop));

    auto* playPauseCmd = actionManager->registerAction(m_playPause, Constants::Actions::PlayPause);
    playPauseCmd->setDescription(tr("Play/Pause"));
    playPauseCmd->setDefaultShortcut(Qt::Key_Space);
    playPauseCmd->setAttribute(ProxyAction::UpdateText);
    playPauseCmd->setAttribute(ProxyAction::UpdateIcon);
    playbackMenu->addAction(playPauseCmd);

    playbackMenu->addAction(actionManager->registerAction(m_next, Constants::Actions::Next));
    playbackMenu->addAction(actionManager->registerAction(m_previous, Constants::Actions::Previous));

    QObject::connect(m_stop, &QAction::triggered, playerController, &PlayerController::stop);
    QObject::connect(m_playPause, &QAction::triggered, playerController, &PlayerController::playPause);
    QObject::connect(m_next, &QAction::triggered, playerController, &PlayerController::next);
    QObject::connect(m_previous, &QAction::triggered, playerController, &PlayerController::previous);

    playbackMenu->addSeparator();

    auto* orderMenu = m_actionManager->createMenu(Constants::Menus::PlaybackOrder);
    orderMenu->menu()->setTitle(tr("&Order"));
    playbackMenu->addMenu(orderMenu);

    m_defaultPlayback->setCheckable(true);
    m_repeatTrack->setCheckable(true);
    m_repeatPlaylist->setCheckable(true);
    m_shuffle->setCheckable(true);

    orderMenu->addAction(actionManager->registerAction(m_defaultPlayback, Constants::Actions::PlaybackDefault));
    orderMenu->addAction(m_actionManager->registerAction(m_repeatTrack, Constants::Actions::RepeatTrack));
    orderMenu->addAction(actionManager->registerAction(m_repeatPlaylist, Constants::Actions::RepeatPlaylist));
    orderMenu->addAction(actionManager->registerAction(m_shuffle, Constants::Actions::ShuffleTracks));

    QObject::connect(m_defaultPlayback, &QAction::triggered, this, [this]() { setPlayMode(Playlist::Default); });
    QObject::connect(m_repeatTrack, &QAction::triggered, this,
                     [this]() { setPlayMode(Playlist::PlayMode::RepeatTrack); });
    QObject::connect(m_repeatPlaylist, &QAction::triggered, this,
                     [this]() { setPlayMode(Playlist::PlayMode::RepeatPlaylist); });
    QObject::connect(m_shuffle, &QAction::triggered, this,
                     [this]() { setPlayMode(Playlist::PlayMode::ShuffleTracks); });

    auto* followPlayback = new QAction(tr("Cursor follows play&back"), this);
    auto* followCursor   = new QAction(tr("Playback follows &cursor"), this);

    auto* stopCurrentCmd = actionManager->registerAction(m_stopAfterCurrent, Constants::Actions::StopAfterCurrent);

    m_stopAfterCurrent->setCheckable(true);
    followPlayback->setCheckable(true);
    followCursor->setCheckable(true);

    m_stopAfterCurrent->setChecked(m_settings->value<Settings::Core::StopAfterCurrent>());
    followPlayback->setChecked(m_settings->value<Settings::Gui::CursorFollowsPlayback>());
    followCursor->setChecked(m_settings->value<Settings::Gui::PlaybackFollowsCursor>());

    QObject::connect(m_stopAfterCurrent, &QAction::triggered, this,
                     [this](bool enabled) { m_settings->set<Settings::Core::StopAfterCurrent>(enabled); });
    QObject::connect(followPlayback, &QAction::triggered, this,
                     [this](bool enabled) { m_settings->set<Settings::Gui::CursorFollowsPlayback>(enabled); });
    QObject::connect(followCursor, &QAction::triggered, this,
                     [this](bool enabled) { m_settings->set<Settings::Gui::PlaybackFollowsCursor>(enabled); });

    m_settings->subscribe<Settings::Core::StopAfterCurrent>(
        this, [this](bool enabled) { m_stopAfterCurrent->setChecked(enabled); });
    m_settings->subscribe<Settings::Gui::CursorFollowsPlayback>(
        this, [followPlayback](bool enabled) { followPlayback->setChecked(enabled); });
    m_settings->subscribe<Settings::Gui::PlaybackFollowsCursor>(
        this, [followCursor](bool enabled) { followCursor->setChecked(enabled); });

    playbackMenu->addAction(stopCurrentCmd->action());
    playbackMenu->addAction(followPlayback);
    playbackMenu->addAction(followCursor);

    updatePlayPause(m_playerController->playState());
    updatePlayMode(m_playerController->playMode());
}

void PlaybackMenu::updatePlayPause(Player::PlayState state) const
{
    if(state == Player::PlayState::Playing) {
        m_playPause->setText(tr("Pause"));
        m_playPause->setIcon(m_pauseIcon);
    }
    else {
        m_playPause->setText(tr("Play"));
        m_playPause->setIcon(m_playIcon);
    }
}

void PlaybackMenu::updatePlayMode(Playlist::PlayModes mode) const
{
    m_repeatTrack->setChecked(mode & Playlist::RepeatTrack);
    m_repeatPlaylist->setChecked(mode & Playlist::RepeatPlaylist);
    m_shuffle->setChecked(mode & Playlist::ShuffleTracks);

    if(mode == 0) {
        m_defaultPlayback->setChecked(true);
        m_repeatTrack->setChecked(false);
        m_repeatPlaylist->setChecked(false);
        m_shuffle->setChecked(false);
    }
    else {
        m_defaultPlayback->setChecked(false);
    }
}

void PlaybackMenu::setPlayMode(Playlist::PlayMode mode) const
{
    auto currentMode    = m_playerController->playMode();
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

    m_playerController->setPlayMode(currentMode);

    if(noChange) {
        updatePlayMode(m_playerController->playMode());
    }
}
} // namespace Fooyin

#include "moc_playbackmenu.cpp"
