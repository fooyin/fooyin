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

using namespace Qt::StringLiterals;

namespace Fooyin {
PlayerControl::PlayerControl(ActionManager* actionManager, PlayerController* playerController,
                             SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_stop{new ToolButton(this)}
    , m_prev{new ToolButton(this)}
    , m_playPause{new ToolButton(this)}
    , m_next{new ToolButton(this)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(m_stop);
    layout->addWidget(m_prev);
    layout->addWidget(m_playPause);
    layout->addWidget(m_next);

    if(auto* stopCmd = m_actionManager->command(Constants::Actions::Stop)) {
        m_stop->setDefaultAction(stopCmd->action());
    }
    if(auto* prevCmd = m_actionManager->command(Constants::Actions::Previous)) {
        m_prev->setDefaultAction(prevCmd->action());
    }
    if(auto* playCmd = m_actionManager->command(Constants::Actions::PlayPause)) {
        m_playPause->setDefaultAction(playCmd->action());
    }
    if(auto* nextCmd = m_actionManager->command(Constants::Actions::Next)) {
        m_next->setDefaultAction(nextCmd->action());
    }

    updateButtonStyle();

    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &PlayerControl::stateChanged);

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { updateIcons(); });
    settings->subscribe<Settings::Gui::ToolButtonStyle>(this, [this]() { updateButtonStyle(); });
}

QString PlayerControl::name() const
{
    return tr("Player Controls");
}

QString PlayerControl::layoutName() const
{
    return u"PlayerControls"_s;
}

void PlayerControl::updateButtonStyle() const
{
    const auto options
        = static_cast<Settings::Gui::ToolButtonOptions>(m_settings->value<Settings::Gui::ToolButtonStyle>());

    m_stop->setStretchEnabled(options & Settings::Gui::Stretch);
    m_stop->setAutoRaise(!(options & Settings::Gui::Raise));

    m_prev->setStretchEnabled(options & Settings::Gui::Stretch);
    m_prev->setAutoRaise(!(options & Settings::Gui::Raise));

    m_playPause->setStretchEnabled(options & Settings::Gui::Stretch);
    m_playPause->setAutoRaise(!(options & Settings::Gui::Raise));

    m_next->setStretchEnabled(options & Settings::Gui::Stretch);
    m_next->setAutoRaise(!(options & Settings::Gui::Raise));
}

void PlayerControl::updateIcons() const
{
    m_stop->setIcon(Utils::iconFromTheme(Constants::Icons::Stop));
    m_prev->setIcon(Utils::iconFromTheme(Constants::Icons::Prev));
    m_next->setIcon(Utils::iconFromTheme(Constants::Icons::Next));
    stateChanged(m_playerController->playState());
}

void PlayerControl::stateChanged(Player::PlayState state) const
{
    switch(state) {
        case(Player::PlayState::Stopped):
            m_playPause->setIcon(Utils::iconFromTheme(Constants::Icons::Play));
            break;
        case(Player::PlayState::Playing):
            m_playPause->setIcon(Utils::iconFromTheme(Constants::Icons::Pause));
            break;
        case(Player::PlayState::Paused):
            m_playPause->setIcon(Utils::iconFromTheme(Constants::Icons::Play));
            break;
    }
}
} // namespace Fooyin

#include "moc_playercontrol.cpp"
