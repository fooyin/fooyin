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
#include <utils/enumhelper.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>

constexpr QSize IconSize = {20, 20};

namespace Fy::Gui::Widgets {
struct PlaylistControl::Private
{
    Core::Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;

    Utils::ComboIcon* repeat;
    Utils::ComboIcon* shuffle;

    Private(PlaylistControl* self, Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings)
        : playerManager{playerManager}
        , settings{settings}
        , repeat{new Utils::ComboIcon(Constants::Icons::RepeatAll, Utils::ComboIcon::HasActiveIcon, self)}
        , shuffle{new Utils::ComboIcon(Constants::Icons::Shuffle, Utils::ComboIcon::HasActiveIcon, self)}
    { }

    void repeatClicked() const
    {
        const auto mode = playerManager->playMode();

        switch(mode) {
            case(Core::Player::PlayMode::Repeat):
                playerManager->setPlayMode(Core::Player::PlayMode::Default);
                break;
            case(Core::Player::PlayMode::RepeatAll):
                playerManager->setPlayMode(Core::Player::PlayMode::Repeat);
                break;
            case(Core::Player::PlayMode::Shuffle):
            case(Core::Player::PlayMode::Default):
                playerManager->setPlayMode(Core::Player::PlayMode::RepeatAll);
                break;
        }
    }

    void shuffleClicked() const
    {
        const auto mode = playerManager->playMode();

        if(mode == Core::Player::PlayMode::Shuffle) {
            playerManager->setPlayMode(Core::Player::PlayMode::Default);
        }
        else {
            playerManager->setPlayMode(Core::Player::PlayMode::Shuffle);
        }
    }

    void setMode(Core::Player::PlayMode mode) const
    {
        switch(mode) {
            case(Core::Player::PlayMode::Repeat):
                repeat->setIcon(Constants::Icons::Repeat, true);
                shuffle->setIcon(Constants::Icons::Shuffle);
                break;
            case(Core::Player::PlayMode::RepeatAll):
                repeat->setIcon(Constants::Icons::RepeatAll, true);
                shuffle->setIcon(Constants::Icons::Shuffle);
                break;
            case(Core::Player::PlayMode::Shuffle):
                shuffle->setIcon(Constants::Icons::Shuffle, true);
                repeat->setIcon(Constants::Icons::RepeatAll);
                break;
            case(Core::Player::PlayMode::Default):
                repeat->setIcon(Constants::Icons::RepeatAll);
                shuffle->setIcon(Constants::Icons::Shuffle);
                break;
        }
    }
};

PlaylistControl::PlaylistControl(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                                 QWidget* parent)
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

    QObject::connect(p->repeat, &Utils::ComboIcon::clicked, this, [this]() { p->repeatClicked(); });
    QObject::connect(p->shuffle, &Utils::ComboIcon::clicked, this, [this]() { p->shuffleClicked(); });
    QObject::connect(playerManager, &Core::Player::PlayerManager::playModeChanged, this,
                     [this](Core::Player::PlayMode mode) { p->setMode(mode); });

    settings->subscribe<Settings::IconTheme>(this, [this]() {
        p->repeat->updateIcons();
        p->shuffle->updateIcons();
    });
}

PlaylistControl::~PlaylistControl() = default;
} // namespace Fy::Gui::Widgets
