/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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
#include <gui/iconloader.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QContextMenuEvent>
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
    , m_stop{new ToolButton(settings, this)}
    , m_prev{new ToolButton(settings, this)}
    , m_pause{new ToolButton(settings, this)}
    , m_play{new ToolButton(settings, this)}
    , m_playPause{new ToolButton(settings, this)}
    , m_next{new ToolButton(settings, this)}
    , m_randomTrack{new ToolButton(settings, this)}
    , m_showStop{true}
    , m_showPrev{true}
    , m_showPause{false}
    , m_showPlay{false}
    , m_showPlayPause{true}
    , m_showNext{true}
    , m_showRandomTrack{false}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSpacing(0);

    layout->addWidget(m_stop);
    layout->addWidget(m_prev);
    layout->addWidget(m_pause);
    layout->addWidget(m_play);
    layout->addWidget(m_playPause);
    layout->addWidget(m_next);
    layout->addWidget(m_randomTrack);

    if(auto* stopCmd = m_actionManager->command(Constants::Actions::Stop)) {
        m_stop->setDefaultAction(stopCmd->action());
    }
    if(auto* prevCmd = m_actionManager->command(Constants::Actions::Previous)) {
        m_prev->setDefaultAction(prevCmd->action());
    }
    if(auto* pauseCmd = m_actionManager->command(Constants::Actions::Pause)) {
        m_pause->setDefaultAction(pauseCmd->action());
    }
    if(auto* playCmd = m_actionManager->command(Constants::Actions::Play)) {
        m_play->setDefaultAction(playCmd->action());
    }
    if(auto* playCmd = m_actionManager->command(Constants::Actions::PlayPause)) {
        m_playPause->setDefaultAction(playCmd->action());
    }
    if(auto* nextCmd = m_actionManager->command(Constants::Actions::Next)) {
        m_next->setDefaultAction(nextCmd->action());
    }
    if(auto* randomTrackCmd = m_actionManager->command(Constants::Actions::RandomTrack)) {
        m_randomTrack->setDefaultAction(randomTrackCmd->action());
    }

    m_pause->hide();
    m_play->hide();
    m_randomTrack->hide();
    updateIcons();

    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &PlayerControl::stateChanged);

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { updateIcons(); });
}

QString PlayerControl::name() const
{
    return tr("Player Controls");
}

QString PlayerControl::layoutName() const
{
    return u"PlayerControls"_s;
}

void PlayerControl::saveLayoutData(QJsonObject& layout)
{
    layout["ShowStop"_L1]        = m_showStop;
    layout["ShowPrevious"_L1]    = m_showPrev;
    layout["ShowPause"_L1]       = m_showPause;
    layout["ShowPlay"_L1]        = m_showPlay;
    layout["ShowPlayPause"_L1]   = m_showPlayPause;
    layout["ShowNext"_L1]        = m_showNext;
    layout["ShowRandomTrack"_L1] = m_showRandomTrack;
}

void PlayerControl::loadLayoutData(const QJsonObject& layout)
{
    const auto updateButton = [](ToolButton* button, bool& member, const bool value) {
        member = value;
        button->setVisible(value);
    };

    if(layout.contains("ShowStop"_L1)) {
        updateButton(m_stop, m_showStop, layout.value("ShowStop"_L1).toBool());
    }
    if(layout.contains("ShowPrevious"_L1)) {
        updateButton(m_prev, m_showPrev, layout.value("ShowPrevious"_L1).toBool());
    }
    if(layout.contains("ShowPause"_L1)) {
        updateButton(m_pause, m_showPause, layout.value("ShowPause"_L1).toBool());
    }
    if(layout.contains("ShowPlay"_L1)) {
        updateButton(m_play, m_showPlay, layout.value("ShowPlay"_L1).toBool());
    }
    if(layout.contains("ShowPlayPause"_L1)) {
        updateButton(m_playPause, m_showPlayPause, layout.value("ShowPlayPause"_L1).toBool());
    }
    if(layout.contains("ShowNext"_L1)) {
        updateButton(m_next, m_showNext, layout.value("ShowNext"_L1).toBool());
    }
    if(layout.contains("ShowRandomTrack"_L1)) {
        updateButton(m_randomTrack, m_showRandomTrack, layout.value("ShowRandomTrack"_L1).toBool());
    }
}

void PlayerControl::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto setupButtonControl = [this, menu](const QString& title, ToolButton* button, bool* member) {
        auto* action = menu->addAction(title);
        action->setCheckable(true);
        action->setChecked(*member);
        QObject::connect(action, &QAction::triggered, this, [member, button](bool checked) {
            *member = checked;
            button->setVisible(checked);
        });
    };

    setupButtonControl(tr("Show Stop"), m_stop, &m_showStop);
    setupButtonControl(tr("Show Previous"), m_prev, &m_showPrev);
    setupButtonControl(tr("Show Pause"), m_pause, &m_showPause);
    setupButtonControl(tr("Show Play"), m_play, &m_showPlay);
    setupButtonControl(tr("Show Play/Pause"), m_playPause, &m_showPlayPause);
    setupButtonControl(tr("Show Next"), m_next, &m_showNext);
    setupButtonControl(tr("Show Random Track"), m_randomTrack, &m_showRandomTrack);

    menu->popup(event->globalPos());
}

void PlayerControl::updateIcons() const
{
    m_stop->setIcon(Gui::iconFromTheme(Constants::Icons::Stop));
    m_prev->setIcon(Gui::iconFromTheme(Constants::Icons::Prev));
    m_pause->setIcon(Gui::iconFromTheme(Constants::Icons::Pause));
    m_play->setIcon(Gui::iconFromTheme(Constants::Icons::Play));
    m_next->setIcon(Gui::iconFromTheme(Constants::Icons::Next));
    m_randomTrack->setIcon(Gui::iconFromTheme(Constants::Icons::RandomPlay));
    stateChanged(m_playerController->playState());
}

void PlayerControl::stateChanged(Player::PlayState state) const
{
    switch(state) {
        case Player::PlayState::Stopped:
            m_playPause->setIcon(Gui::iconFromTheme(Constants::Icons::Play));
            break;
        case Player::PlayState::Playing:
            m_playPause->setIcon(Gui::iconFromTheme(Constants::Icons::Pause));
            break;
        case Player::PlayState::Paused:
            m_playPause->setIcon(Gui::iconFromTheme(Constants::Icons::Play));
            break;
    }
}
} // namespace Fooyin

#include "moc_playercontrol.cpp"
