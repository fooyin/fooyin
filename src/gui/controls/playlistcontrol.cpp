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
#include <gui/guisettings.h>
#include <utils/comboicon.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>

constexpr QSize IconSize = {20, 20};

namespace Fooyin {
struct PlaylistControl::Private
{
    PlayerManager* playerManager;
    SettingsManager* settings;

    ComboIcon* repeat;
    ComboIcon* shuffle;

    Private(PlaylistControl* self, PlayerManager* playerManager, SettingsManager* settings)
        : playerManager{playerManager}
        , settings{settings}
        , repeat{new ComboIcon(Constants::Icons::RepeatAll, ComboIcon::HasActiveIcon, self)}
        , shuffle{new ComboIcon(Constants::Icons::Shuffle, ComboIcon::HasActiveIcon, self)}
    { }

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
        if(mode & Playlist::Repeat) {
            repeat->setIcon(Constants::Icons::Repeat, true);
        }
        else if(mode & Playlist::RepeatAll) {
            repeat->setIcon(Constants::Icons::RepeatAll, true);
        }
        else {
            repeat->setIcon(Constants::Icons::RepeatAll, false);
        }

        if(mode & Playlist::Shuffle) {
            shuffle->setIcon(Constants::Icons::Shuffle, true);
        }
        else {
            shuffle->setIcon(Constants::Icons::Shuffle, false);
        }
    }
};

PlaylistControl::PlaylistControl(PlayerManager* playerManager, SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}

{
    auto* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setSpacing(10);
    layout->setContentsMargins(0, 0, 0, 0);

    p->repeat->addIcon(Constants::Icons::Repeat);

    p->repeat->setMaximumSize(IconSize);
    p->shuffle->setMaximumSize(IconSize);

    layout->addWidget(p->repeat, 0, Qt::AlignVCenter);
    layout->addWidget(p->shuffle, 0, Qt::AlignVCenter);

    p->setMode(p->playerManager->playMode());

    QObject::connect(p->repeat, &ComboIcon::clicked, this, [this]() { p->repeatClicked(); });
    QObject::connect(p->shuffle, &ComboIcon::clicked, this, [this]() { p->shuffleClicked(); });
    QObject::connect(playerManager, &PlayerManager::playModeChanged, this,
                     [this](Playlist::PlayModes mode) { p->setMode(mode); });

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() {
        p->repeat->updateIcons();
        p->shuffle->updateIcons();
    });
}

PlaylistControl::~PlaylistControl() = default;
} // namespace Fooyin

#include "moc_playlistcontrol.cpp"
