/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>

namespace Fooyin {
PlaylistControl::PlaylistControl(PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_repeat{new ToolButton(this)}
    , m_shuffle{new ToolButton(this)}
    , m_iconColour{palette().highlight().color()}

{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(m_repeat);
    layout->addWidget(m_shuffle);

    m_repeat->setPopupMode(QToolButton::InstantPopup);
    m_shuffle->setPopupMode(QToolButton::InstantPopup);

    auto* repeatAction = new QAction(this);
    repeatAction->setToolTip(tr("Repeat"));
    m_repeat->setDefaultAction(repeatAction);

    auto* shuffleAction = new QAction(this);
    shuffleAction->setToolTip(tr("Shuffle"));
    m_shuffle->setDefaultAction(shuffleAction);

    setMode(playerController->playMode());

    setupMenus();
    updateButtonStyle();

    QObject::connect(playerController, &PlayerController::playModeChanged, this, &PlaylistControl::setMode);

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { setMode(m_playerController->playMode()); });
    settings->subscribe<Settings::Gui::ToolButtonStyle>(this, &PlaylistControl::updateButtonStyle);
}

QString PlaylistControl::name() const
{
    return tr("Playlist Controls");
}

QString PlaylistControl::layoutName() const
{
    return QStringLiteral("PlaylistControls");
}

void PlaylistControl::updateButtonStyle() const
{
    const auto options
        = static_cast<Settings::Gui::ToolButtonOptions>(m_settings->value<Settings::Gui::ToolButtonStyle>());

    m_repeat->setStretchEnabled(options & Settings::Gui::Stretch);
    m_repeat->setAutoRaise(!(options & Settings::Gui::Raise));

    m_shuffle->setStretchEnabled(options & Settings::Gui::Stretch);
    m_shuffle->setAutoRaise(!(options & Settings::Gui::Raise));
}

void PlaylistControl::setupMenus()
{
    auto* repeatMenu = new QMenu(this);

    auto* repeatGroup = new QActionGroup(repeatMenu);

    auto* defaultAction  = new QAction(tr("Default"), repeatGroup);
    auto* repeatPlaylist = new QAction(tr("Repeat playlist"), repeatGroup);
    auto* repeatAlbum    = new QAction(tr("Repeat album"), repeatGroup);
    auto* repeatTrack    = new QAction(tr("Repeat track"), repeatGroup);

    defaultAction->setCheckable(true);
    repeatPlaylist->setCheckable(true);
    repeatAlbum->setCheckable(true);
    repeatTrack->setCheckable(true);

    const auto setRepeatMode = [this](Playlist::PlayModes newMode) {
        const auto pMode = m_playerController->playMode();
        m_playerController->setPlayMode(
            (pMode & ~(Playlist::RepeatTrack | Playlist::RepeatAlbum | Playlist::RepeatPlaylist)) | newMode);
    };

    QObject::connect(defaultAction, &QAction::triggered, this, [setRepeatMode]() { setRepeatMode(Playlist::Default); });
    QObject::connect(repeatAlbum, &QAction::triggered, this,
                     [setRepeatMode]() { setRepeatMode(Playlist::RepeatAlbum); });
    QObject::connect(repeatPlaylist, &QAction::triggered, this,
                     [setRepeatMode]() { setRepeatMode(Playlist::RepeatPlaylist); });
    QObject::connect(repeatTrack, &QAction::triggered, this,
                     [setRepeatMode]() { setRepeatMode(Playlist::RepeatTrack); });

    repeatMenu->addAction(defaultAction);
    repeatMenu->addAction(repeatPlaylist);
    repeatMenu->addAction(repeatAlbum);
    repeatMenu->addAction(repeatTrack);

    m_repeat->setMenu(repeatMenu);

    auto* shuffleMenu = new QMenu(this);

    auto* shuffleGroup = new QActionGroup(shuffleMenu);

    auto* offAction     = new QAction(tr("Shuffle off"), shuffleGroup);
    auto* shuffleTracks = new QAction(tr("Shuffle tracks"), shuffleGroup);
    auto* shuffleAlbums = new QAction(tr("Shuffle albums"), shuffleGroup);
    auto* random        = new QAction(tr("Random"), shuffleGroup);

    offAction->setCheckable(true);
    shuffleTracks->setCheckable(true);
    shuffleAlbums->setCheckable(true);
    random->setCheckable(true);

    const auto clearShuffleMode = [this]() {
        const auto pMode = m_playerController->playMode();
        m_playerController->setPlayMode(pMode
                                        & ~(Playlist::ShuffleTracks | Playlist::ShuffleAlbums | Playlist::Random));
    };
    const auto setShuffleMode = [this](Playlist::PlayModes newMode) {
        const auto pMode = m_playerController->playMode();
        m_playerController->setPlayMode(
            (pMode & ~(Playlist::ShuffleTracks | Playlist::ShuffleAlbums | Playlist::Random)) | newMode);
    };

    QObject::connect(offAction, &QAction::triggered, this, [clearShuffleMode]() { clearShuffleMode(); });
    QObject::connect(shuffleTracks, &QAction::triggered, this,
                     [setShuffleMode]() { setShuffleMode(Playlist::ShuffleTracks); });
    QObject::connect(shuffleAlbums, &QAction::triggered, this,
                     [setShuffleMode]() { setShuffleMode(Playlist::ShuffleAlbums); });
    QObject::connect(random, &QAction::triggered, this, [setShuffleMode]() { setShuffleMode(Playlist::Random); });

    shuffleMenu->addAction(offAction);
    shuffleMenu->addAction(shuffleTracks);
    shuffleMenu->addAction(shuffleAlbums);
    shuffleMenu->addAction(random);

    m_shuffle->setMenu(shuffleMenu);

    const auto updateActions = [repeatPlaylist, repeatAlbum, repeatTrack, defaultAction, shuffleTracks, shuffleAlbums,
                                random, offAction, this]() {
        auto playMode = m_playerController->playMode();

        if(playMode & Playlist::RepeatPlaylist) {
            repeatPlaylist->setChecked(true);
        }
        else if(playMode & Playlist::RepeatAlbum) {
            repeatAlbum->setChecked(true);
        }
        else if(playMode & Playlist::RepeatTrack) {
            repeatTrack->setChecked(true);
        }
        else {
            defaultAction->setChecked(true);
        }

        if(playMode & Playlist::ShuffleTracks) {
            shuffleTracks->setChecked(true);
        }
        else if(playMode & Playlist::ShuffleAlbums) {
            shuffleAlbums->setChecked(true);
        }
        else if(playMode & Playlist::Random) {
            random->setChecked(true);
        }
        else {
            offAction->setChecked(true);
        }
    };

    QObject::connect(m_playerController, &PlayerController::playModeChanged, this, updateActions);
}

void PlaylistControl::setMode(Playlist::PlayModes mode) const
{
    if(mode & (Playlist::RepeatPlaylist | Playlist::RepeatAlbum)) {
        m_repeat->setIcon(
            Utils::changePixmapColour(Utils::iconFromTheme(Constants::Icons::Repeat).pixmap({128, 128}), m_iconColour));
    }
    else if(mode & Playlist::RepeatTrack) {
        m_repeat->setIcon(Utils::changePixmapColour(
            Utils::iconFromTheme(Constants::Icons::RepeatTrack).pixmap({128, 128}), m_iconColour));
    }
    else {
        m_repeat->setIcon(Utils::iconFromTheme(Constants::Icons::Repeat));
    }

    if(mode & (Playlist::ShuffleTracks | Playlist::ShuffleAlbums | Playlist::Random)) {
        m_shuffle->setIcon(Utils::changePixmapColour(Utils::iconFromTheme(Constants::Icons::Shuffle).pixmap({128, 128}),
                                                     m_iconColour));
    }
    else {
        m_shuffle->setIcon(Utils::iconFromTheme(Constants::Icons::Shuffle));
    }
}
} // namespace Fooyin

#include "moc_playlistcontrol.cpp"
