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

#include "playercontrol.h"

#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>

namespace Fooyin {
struct PlayerControl::Private
{
    PlayerControl* self;

    ActionManager* actionManager;
    PlayerController* playerController;
    SettingsManager* settings;

    ToolButton* stop;
    ToolButton* prev;
    ToolButton* playPause;
    ToolButton* next;

    Private(PlayerControl* self_, ActionManager* actionManager_, PlayerController* playerController_,
            SettingsManager* settings_)
        : self{self_}
        , actionManager{actionManager_}
        , playerController{playerController_}
        , settings{settings_}
        , stop{new ToolButton(self)}
        , prev{new ToolButton(self)}
        , playPause{new ToolButton(self)}
        , next{new ToolButton(self)}
    {
        if(auto* stopCmd = actionManager->command(Constants::Actions::Stop)) {
            stop->setDefaultAction(stopCmd->action());
        }
        if(auto* prevCmd = actionManager->command(Constants::Actions::Previous)) {
            prev->setDefaultAction(prevCmd->action());
        }
        if(auto* playCmd = actionManager->command(Constants::Actions::PlayPause)) {
            playPause->setDefaultAction(playCmd->action());
        }
        if(auto* nextCmd = actionManager->command(Constants::Actions::Next)) {
            next->setDefaultAction(nextCmd->action());
        }

        updateButtonStyle();
    }

    void updateButtonStyle() const
    {
        const auto options
            = static_cast<Settings::Gui::ToolButtonOptions>(settings->value<Settings::Gui::ToolButtonStyle>());

        stop->setStretchEnabled(options & Settings::Gui::Stretch);
        stop->setAutoRaise(!(options & Settings::Gui::Raise));

        prev->setStretchEnabled(options & Settings::Gui::Stretch);
        prev->setAutoRaise(!(options & Settings::Gui::Raise));

        playPause->setStretchEnabled(options & Settings::Gui::Stretch);
        playPause->setAutoRaise(!(options & Settings::Gui::Raise));

        next->setStretchEnabled(options & Settings::Gui::Stretch);
        next->setAutoRaise(!(options & Settings::Gui::Raise));
    }

    void updateIcons() const
    {
        stop->setIcon(Utils::iconFromTheme(Constants::Icons::Stop));
        prev->setIcon(Utils::iconFromTheme(Constants::Icons::Prev));
        next->setIcon(Utils::iconFromTheme(Constants::Icons::Next));
        stateChanged(playerController->playState());
    }

    void stateChanged(PlayState state) const
    {
        switch(state) {
            case(PlayState::Stopped):
                playPause->setIcon(Utils::iconFromTheme(Constants::Icons::Play));
                break;
            case(PlayState::Playing):
                playPause->setIcon(Utils::iconFromTheme(Constants::Icons::Pause));
                break;
            case(PlayState::Paused):
                playPause->setIcon(Utils::iconFromTheme(Constants::Icons::Play));
                break;
        }
    }
};

PlayerControl::PlayerControl(ActionManager* actionManager, PlayerController* playerController,
                             SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, playerController, settings)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(p->stop);
    layout->addWidget(p->prev);
    layout->addWidget(p->playPause);
    layout->addWidget(p->next);

    QObject::connect(p->playerController, &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { p->updateIcons(); });
    settings->subscribe<Settings::Gui::ToolButtonStyle>(this, [this]() { p->updateButtonStyle(); });
}

PlayerControl::~PlayerControl() = default;

QString PlayerControl::name() const
{
    return tr("Player Controls");
}

QString PlayerControl::layoutName() const
{
    return QStringLiteral("PlayerControls");
}
} // namespace Fooyin

#include "moc_playercontrol.cpp"
