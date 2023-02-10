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

#include <core/constants.h>
#include <core/player/playermanager.h>
#include <utils/comboicon.h>

#include <QHBoxLayout>

namespace Gui::Widgets {
PlayerControl::PlayerControl(QWidget* parent)
    : QWidget{parent}
    , m_layout{new QHBoxLayout(this)}
    , m_stop{new Utils::ComboIcon(Core::Constants::Icons::Stop, this)}
    , m_prev{new Utils::ComboIcon(Core::Constants::Icons::Prev, this)}
    , m_play{new Utils::ComboIcon(Core::Constants::Icons::Play, this)}
    , m_next{new Utils::ComboIcon(Core::Constants::Icons::Next, this)}
    , m_labelSize{20, 20}
{
    setupUi();

    connect(m_stop, &Utils::ComboIcon::clicked, this, &PlayerControl::stopClicked);
    connect(m_prev, &Utils::ComboIcon::clicked, this, &PlayerControl::prevClicked);
    connect(m_play, &Utils::ComboIcon::clicked, this, &PlayerControl::pauseClicked);
    connect(m_next, &Utils::ComboIcon::clicked, this, &PlayerControl::nextClicked);
}

void PlayerControl::setupUi()
{
    m_layout->setSizeConstraint(QLayout::SetFixedSize);
    m_layout->setSpacing(10);
    m_layout->setContentsMargins(10, 0, 0, 0);

    m_play->addPixmap(Core::Constants::Icons::Pause);

    m_stop->setMaximumSize(m_labelSize);
    m_prev->setMaximumSize(m_labelSize);
    m_play->setMaximumSize(m_labelSize);
    m_next->setMaximumSize(m_labelSize);

    m_layout->addWidget(m_stop, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_prev, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_play, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_next, 0, Qt::AlignVCenter);

    setEnabled(false);
}

void PlayerControl::stateChanged(Core::Player::PlayState state)
{
    switch(state) {
        case(Core::Player::Stopped):
            setEnabled(false);
            return m_play->setIcon(Core::Constants::Icons::Play);
        case(Core::Player::Playing):
            setEnabled(true);
            return m_play->setIcon(Core::Constants::Icons::Pause);
        case(Core::Player::Paused):
            return m_play->setIcon(Core::Constants::Icons::Play);
    }
}
} // namespace Gui::Widgets
