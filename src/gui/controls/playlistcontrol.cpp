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

#include <core/coresettings.h>
#include <core/player/playermanager.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QActionGroup>
#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>

namespace Fooyin {
struct PlaylistControl::Private
{
    PlaylistControl* self;

    PlayerManager* playerManager;
    SettingsManager* settings;

    ToolButton* repeat;
    ToolButton* shuffle;

    QIcon repeatActiveIcon;
    QIcon shuffleActiveIcon;

    Private(PlaylistControl* self_, PlayerManager* playerManager_, SettingsManager* settings_)
        : self{self_}
        , playerManager{playerManager_}
        , settings{settings_}
        , repeat{new ToolButton(self)}
        , shuffle{new ToolButton(self)}
        , repeatActiveIcon{Utils::changePixmapColour(Utils::iconFromTheme(Constants::Icons::Repeat).pixmap({128, 128}),
                                                     self->palette().highlight().color())}
        , shuffleActiveIcon{Utils::changePixmapColour(
              Utils::iconFromTheme(Constants::Icons::Shuffle).pixmap({128, 128}), self->palette().highlight().color())}
    {
        repeat->setPopupMode(QToolButton::InstantPopup);

        auto* repeatAction = new QAction(self);
        repeatAction->setToolTip(QStringLiteral("Repeat"));
        repeat->setDefaultAction(repeatAction);

        auto* shuffleAction = new QAction(self);
        shuffleAction->setToolTip(QStringLiteral("Shuffle"));
        shuffle->setDefaultAction(shuffleAction);

        repeat->setAutoRaise(true);
        shuffle->setAutoRaise(true);

        setMode(playerManager->playMode());

        setupMenus();
    }

    void setupMenus()
    {
        auto* menu = new QMenu(self);

        auto* repeatGroup = new QActionGroup(menu);

        auto* defaultAction  = new QAction(tr("Default"), repeatGroup);
        auto* repeatPlaylist = new QAction(tr("Repeat playlist"), repeatGroup);
        auto* repeatTrack    = new QAction(tr("Repeat track"), repeatGroup);

        defaultAction->setCheckable(true);
        repeatPlaylist->setCheckable(true);
        repeatTrack->setCheckable(true);

        auto playMode = playerManager->playMode();

        if(playMode & Playlist::RepeatAll) {
            repeatPlaylist->setChecked(true);
        }
        else if(playMode & Playlist::Repeat) {
            repeatTrack->setChecked(true);
        }
        else {
            defaultAction->setChecked(true);
        }

        QObject::connect(defaultAction, &QAction::triggered, self, [this, playMode]() {
            playerManager->setPlayMode(playMode & ~Playlist::Repeat & ~Playlist::RepeatAll);
        });

        QObject::connect(repeatPlaylist, &QAction::triggered, self, [this, playMode]() {
            playerManager->setPlayMode((playMode & ~Playlist::Repeat) | Playlist::RepeatAll);
        });

        QObject::connect(repeatTrack, &QAction::triggered, self, [this, playMode]() {
            playerManager->setPlayMode((playMode & ~Playlist::RepeatAll) | Playlist::Repeat);
        });

        menu->addAction(defaultAction);
        menu->addAction(repeatPlaylist);
        menu->addAction(repeatTrack);

        repeat->setMenu(menu);
    }

    void repeatClicked() const
    {
        Playlist::PlayModes mode = playerManager->playMode();

        if(mode & Playlist::RepeatAll) {
            mode &= ~Playlist::RepeatAll;
            mode |= Playlist::Repeat;
        }
        else if(mode & Playlist::Repeat) {
            mode &= ~Playlist::Repeat;
        }
        else {
            mode |= Playlist::RepeatAll;
        }

        playerManager->setPlayMode(mode);
    }

    void shuffleClicked() const
    {
        Playlist::PlayModes mode = playerManager->playMode();

        if(mode & Playlist::Shuffle) {
            mode &= ~Playlist::Shuffle;
        }
        else {
            mode |= Playlist::Shuffle;
        }

        playerManager->setPlayMode(mode);
    }

    void setMode(Playlist::PlayModes mode) const
    {
        if(mode & Playlist::Repeat || mode & Playlist::RepeatAll) {
            repeat->setIcon(repeatActiveIcon);
        }
        else {
            repeat->setIcon(Utils::iconFromTheme(Constants::Icons::Repeat));
        }

        if(mode & Playlist::Shuffle) {
            shuffle->setIcon(shuffleActiveIcon);
        }
        else {
            shuffle->setIcon(Utils::iconFromTheme(Constants::Icons::Shuffle));
        }
    }
};

PlaylistControl::PlaylistControl(PlayerManager* playerManager, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}

{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(p->repeat);
    layout->addWidget(p->shuffle);

    QObject::connect(p->shuffle, &QToolButton::clicked, this, [this]() { p->shuffleClicked(); });
    QObject::connect(playerManager, &PlayerManager::playModeChanged, this,
                     [this](Playlist::PlayModes mode) { p->setMode(mode); });

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { p->setMode(p->playerManager->playMode()); });
}

PlaylistControl::~PlaylistControl() = default;

QString PlaylistControl::name() const
{
    return QStringLiteral("Playlist Controls");
}

QString PlaylistControl::layoutName() const
{
    return QStringLiteral("PlaylistControls");
}
} // namespace Fooyin

#include "moc_playlistcontrol.cpp"
