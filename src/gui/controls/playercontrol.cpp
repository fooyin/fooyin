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

#include <core/player/playermanager.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/comboicon.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>

constexpr QSize IconSize = {20, 20};

namespace Fy::Gui::Widgets {
using Utils::ComboIcon;

struct PlayerControl::Private
{
    PlayerControl* self;

    Core::Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;

    Utils::ComboIcon* stop;
    Utils::ComboIcon* prev;
    Utils::ComboIcon* playPause;
    Utils::ComboIcon* next;

    Private(PlayerControl* self, Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings)
        : self{self}
        , playerManager{playerManager}
        , settings{settings}
        , stop{new ComboIcon(Constants::Icons::Stop, ComboIcon::HasDisabledIcon, self)}
        , prev{new ComboIcon(Constants::Icons::Prev, ComboIcon::HasDisabledIcon, self)}
        , playPause{new ComboIcon(Constants::Icons::Play, ComboIcon::HasDisabledIcon, self)}
        , next{new ComboIcon(Constants::Icons::Next, ComboIcon::HasDisabledIcon, self)}
    {
        QObject::connect(stop, &ComboIcon::clicked, playerManager, &Core::Player::PlayerManager::stop);
        QObject::connect(prev, &ComboIcon::clicked, playerManager, &Core::Player::PlayerManager::previous);
        QObject::connect(playPause, &ComboIcon::clicked, playerManager, &Core::Player::PlayerManager::playPause);
        QObject::connect(next, &ComboIcon::clicked, playerManager, &Core::Player::PlayerManager::next);

        stateChanged(playerManager->playState());
    }

    void stateChanged(Core::Player::PlayState state) const
    {
        switch(state) {
            case(Core::Player::PlayState::Stopped): {
                playPause->setIcon(Constants::Icons::Play);
                break;
            }
            case(Core::Player::PlayState::Playing): {
                playPause->setIcon(Constants::Icons::Pause);
                break;
            }
            case(Core::Player::PlayState::Paused): {
                playPause->setIcon(Constants::Icons::Play);
                break;
            }
        }
    }
};

PlayerControl::PlayerControl(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                             QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 0, 0, 0);

    p->playPause->addIcon(Constants::Icons::Pause);

    p->stop->setMaximumSize(IconSize);
    p->prev->setMaximumSize(IconSize);
    p->playPause->setMaximumSize(IconSize);
    p->next->setMaximumSize(IconSize);

    layout->addWidget(p->stop, 0, Qt::AlignVCenter);
    layout->addWidget(p->prev, 0, Qt::AlignVCenter);
    layout->addWidget(p->playPause, 0, Qt::AlignVCenter);
    layout->addWidget(p->next, 0, Qt::AlignVCenter);

    QObject::connect(p->playerManager, &Core::Player::PlayerManager::playStateChanged, this,
                     [this](Core::Player::PlayState state) { p->stateChanged(state); });

    settings->subscribe<Settings::IconTheme>(this, [this]() {
        p->stop->updateIcons();
        p->prev->updateIcons();
        p->playPause->updateIcons();
        p->next->updateIcons();
    });
}

PlayerControl::~PlayerControl() = default;
} // namespace Fy::Gui::Widgets

#include "moc_playercontrol.cpp"
