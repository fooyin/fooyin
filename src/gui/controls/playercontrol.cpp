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

#include "playercontrol.h"

#include <core/player/playermanager.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/toolbutton.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>
#include <QToolButton>

constexpr QSize IconSize = {20, 20};

namespace Fooyin {
struct PlayerControl::Private
{
    PlayerControl* self;

    PlayerManager* playerManager;
    SettingsManager* settings;

    ToolButton* stop;
    ToolButton* prev;
    ToolButton* playPause;
    ToolButton* next;

    Private(PlayerControl* self, PlayerManager* playerManager, SettingsManager* settings)
        : self{self}
        , playerManager{playerManager}
        , settings{settings}
        , stop{new ToolButton(self)}
        , prev{new ToolButton(self)}
        , playPause{new ToolButton(self)}
        , next{new ToolButton(self)}
    {
        QObject::connect(stop, &QToolButton::clicked, playerManager, &PlayerManager::stop);
        QObject::connect(prev, &QToolButton::clicked, playerManager, &PlayerManager::previous);
        QObject::connect(playPause, &QToolButton::clicked, playerManager, &PlayerManager::playPause);
        QObject::connect(next, &QToolButton::clicked, playerManager, &PlayerManager::next);

        stop->setIconSize(IconSize);
        prev->setIconSize(IconSize);
        playPause->setIconSize(IconSize);
        next->setIconSize(IconSize);

        stop->setMaximumSize(IconSize);
        prev->setMaximumSize(IconSize);
        playPause->setMaximumSize(IconSize);
        next->setMaximumSize(IconSize);

        stop->setAutoRaise(true);
        prev->setAutoRaise(true);
        playPause->setAutoRaise(true);
        next->setAutoRaise(true);

        updateIcons();
    }

    void updateIcons() const
    {
        stop->setIcon(QIcon::fromTheme(Constants::Icons::Stop));
        prev->setIcon(QIcon::fromTheme(Constants::Icons::Prev));
        next->setIcon(QIcon::fromTheme(Constants::Icons::Next));
        stateChanged(playerManager->playState());
    }

    void stateChanged(PlayState state) const
    {
        switch(state) {
            case(PlayState::Stopped): {
                playPause->setIcon(QIcon::fromTheme(Constants::Icons::Play));
                break;
            }
            case(PlayState::Playing): {
                playPause->setIcon(QIcon::fromTheme(Constants::Icons::Pause));
                break;
            }
            case(PlayState::Paused): {
                playPause->setIcon(QIcon::fromTheme(Constants::Icons::Play));
                break;
            }
        }
    }
};

PlayerControl::PlayerControl(PlayerManager* playerManager, SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 0, 0, 0);

    layout->addWidget(p->stop, 0, Qt::AlignVCenter);
    layout->addWidget(p->prev, 0, Qt::AlignVCenter);
    layout->addWidget(p->playPause, 0, Qt::AlignVCenter);
    layout->addWidget(p->next, 0, Qt::AlignVCenter);

    QObject::connect(p->playerManager, &PlayerManager::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { p->updateIcons(); });
}

PlayerControl::~PlayerControl() = default;
} // namespace Fooyin

#include "moc_playercontrol.cpp"
