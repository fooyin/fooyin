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

#include "gui/widgets/comboicon.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <core/plugins/pluginmanager.h>

#include <QHBoxLayout>

namespace Gui::Widgets {
struct PlaylistControl::Private
{
    Core::SettingsManager* settings{PluginSystem::object<Core::SettingsManager>()};
    QHBoxLayout* layout;

    QSize labelSize{20, 20};

    ComboIcon* repeat;
    ComboIcon* shuffle;
};

PlaylistControl::PlaylistControl(QWidget* parent)
    : QWidget(parent)
    , p(std::make_unique<Private>())
{
    setupUi();

    connect(p->repeat, &ComboIcon::clicked, this, &PlaylistControl::repeatClicked);
    connect(p->shuffle, &ComboIcon::clicked, this, &PlaylistControl::shuffleClicked);
}

PlaylistControl::~PlaylistControl() = default;

void PlaylistControl::setupUi()
{
    p->layout = new QHBoxLayout(this);

    p->layout->setSizeConstraint(QLayout::SetFixedSize);
    p->layout->setSpacing(10);
    p->layout->setContentsMargins(0, 0, 0, 0);

    p->repeat  = new ComboIcon(Core::Constants::Icons::RepeatAll, Core::Fy::HasActiveIcon, this);
    p->shuffle = new ComboIcon(Core::Constants::Icons::Shuffle, Core::Fy::HasActiveIcon, this);

    p->repeat->addPixmap(Core::Constants::Icons::Repeat);

    p->repeat->setMaximumSize(p->labelSize);
    p->shuffle->setMaximumSize(p->labelSize);

    p->layout->addWidget(p->repeat, 0, Qt::AlignVCenter);
    p->layout->addWidget(p->shuffle, 0, Qt::AlignVCenter);

    const auto mode = static_cast<Core::Player::PlayMode>(p->settings->value<Core::Settings::PlayMode>().toInt());
    setMode(mode);
}

void PlaylistControl::playModeChanged(Core::Player::PlayMode mode)
{
    p->settings->set<Core::Settings::PlayMode>(mode);
    setMode(mode);
}

void PlaylistControl::setMode(Core::Player::PlayMode mode) const
{
    switch(mode) {
        case(Core::Player::PlayMode::Repeat): {
            p->repeat->setIcon(Core::Constants::Icons::Repeat, true);
            p->shuffle->setIcon(Core::Constants::Icons::Shuffle);
            break;
        }
        case(Core::Player::PlayMode::RepeatAll): {
            p->repeat->setIcon(Core::Constants::Icons::RepeatAll, true);
            p->shuffle->setIcon(Core::Constants::Icons::Shuffle);
            break;
        }
        case(Core::Player::PlayMode::Shuffle): {
            p->shuffle->setIcon(Core::Constants::Icons::Shuffle, true);
            p->repeat->setIcon(Core::Constants::Icons::RepeatAll);
            break;
        }
        case(Core::Player::PlayMode::Default): {
            p->repeat->setIcon(Core::Constants::Icons::RepeatAll);
            p->shuffle->setIcon(Core::Constants::Icons::Shuffle);
            break;
        }
        default:
            return;
    }
}
} // namespace Gui::Widgets
