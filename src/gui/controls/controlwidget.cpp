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

#include "controlwidget.h"

#include "playercontrol.h"
#include "playlistcontrol.h"
#include "progresswidget.h"
#include "volumecontrol.h"

#include <core/player/playermanager.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>

namespace Fy::Gui::Widgets {
struct ControlWidget::Private
{
    ControlWidget* self;

    Core::Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;

    PlayerControl* playerControls;
    PlaylistControl* playlistControls;
    VolumeControl* volumeControls;
    ProgressWidget* progress;

    Private(ControlWidget* self, Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings)
        : self{self}
        , playerManager{playerManager}
        , settings{settings}
        , playerControls{new PlayerControl(playerManager, settings, self)}
        , playlistControls{new PlaylistControl(playerManager, settings, self)}
        , volumeControls{new VolumeControl(settings, self)}
        , progress{new ProgressWidget(playerManager, settings, self)}
    { }
};

ControlWidget::ControlWidget(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                             QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    setObjectName("Control Bar");

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(p->playerControls, 0, Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(p->progress, 0, Qt::AlignVCenter);
    layout->addWidget(p->playlistControls, 0, Qt::AlignVCenter);
    layout->addWidget(p->volumeControls, 0, Qt::AlignVCenter);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(15);
}

ControlWidget::~ControlWidget() = default;

QString ControlWidget::name() const
{
    return QStringLiteral("Controls");
}
} // namespace Fy::Gui::Widgets
