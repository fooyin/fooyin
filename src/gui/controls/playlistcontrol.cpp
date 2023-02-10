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

#include <core/constants.h>
#include <core/coresettings.h>
#include <core/player/playermanager.h>
#include <utils/comboicon.h>

#include <QHBoxLayout>

namespace Gui::Widgets {
PlaylistControl::PlaylistControl(Core::SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(this)}
    , m_labelSize{20, 20}
    , m_repeat{new Utils::ComboIcon(Core::Constants::Icons::RepeatAll, Utils::ComboIcon::HasActiveIcon, this)}
    , m_shuffle{new Utils::ComboIcon(Core::Constants::Icons::Shuffle, Utils::ComboIcon::HasActiveIcon, this)}

{
    setupUi();

    connect(m_repeat, &Utils::ComboIcon::clicked, this, &PlaylistControl::repeatClicked);
    connect(m_shuffle, &Utils::ComboIcon::clicked, this, &PlaylistControl::shuffleClicked);
}

void PlaylistControl::setupUi()
{
    m_layout->setSizeConstraint(QLayout::SetFixedSize);
    m_layout->setSpacing(10);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_repeat->addPixmap(Core::Constants::Icons::Repeat);

    m_repeat->setMaximumSize(m_labelSize);
    m_shuffle->setMaximumSize(m_labelSize);

    m_layout->addWidget(m_repeat, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_shuffle, 0, Qt::AlignVCenter);

    const auto mode = static_cast<Core::Player::PlayMode>(m_settings->value<Core::Settings::PlayMode>().toInt());
    setMode(mode);
}

void PlaylistControl::playModeChanged(Core::Player::PlayMode mode)
{
    m_settings->set<Core::Settings::PlayMode>(mode);
    setMode(mode);
}

void PlaylistControl::setMode(Core::Player::PlayMode mode) const
{
    switch(mode) {
        case(Core::Player::Repeat): {
            m_repeat->setIcon(Core::Constants::Icons::Repeat, true);
            m_shuffle->setIcon(Core::Constants::Icons::Shuffle);
            break;
        }
        case(Core::Player::RepeatAll): {
            m_repeat->setIcon(Core::Constants::Icons::RepeatAll, true);
            m_shuffle->setIcon(Core::Constants::Icons::Shuffle);
            break;
        }
        case(Core::Player::Shuffle): {
            m_shuffle->setIcon(Core::Constants::Icons::Shuffle, true);
            m_repeat->setIcon(Core::Constants::Icons::RepeatAll);
            break;
        }
        case(Core::Player::Default): {
            m_repeat->setIcon(Core::Constants::Icons::RepeatAll);
            m_shuffle->setIcon(Core::Constants::Icons::Shuffle);
            break;
        }
        default:
            return;
    }
}
} // namespace Gui::Widgets
