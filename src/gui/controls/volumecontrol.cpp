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

#include "volumecontrol.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/hovermenu.h>
#include <gui/widgets/logslider.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

#include <chrono>

using namespace std::chrono_literals;

constexpr double MinVolume = 0.01;

namespace Fooyin {
struct VolumeControl::Private
{
    VolumeControl* self;

    ActionManager* actionManager;
    SettingsManager* settings;

    ToolButton* volumeIcon;
    HoverMenu* volumeMenu;
    LogSlider* volumeSlider;

    Private(VolumeControl* self_, ActionManager* actionManager_, SettingsManager* settings_)
        : self{self_}
        , actionManager{actionManager_}
        , settings{settings_}
        , volumeIcon{new ToolButton(self)}
        , volumeMenu{new HoverMenu(self)}
        , volumeSlider{new LogSlider(Qt::Vertical, self)}
    {
        auto* volumeLayout = new QVBoxLayout(volumeMenu);
        volumeLayout->addWidget(volumeSlider);

        if(auto* muteCmd = actionManager->command(Constants::Actions::Mute)) {
            volumeIcon->setDefaultAction(muteCmd->action());
        }

        volumeIcon->setAutoRaise(true);

        volumeSlider->setMinimumHeight(100);
        volumeSlider->setRange(MinVolume, 1.0);
        volumeSlider->setNaturalValue(settings->value<Settings::Core::OutputVolume>());

        volumeMenu->hide();

        updateDisplay(settings->value<Settings::Core::OutputVolume>());
    }

    void showVolumeMenu() const
    {
        const int menuWidth  = volumeMenu->sizeHint().width();
        const int menuHeight = volumeMenu->sizeHint().height();

        const int yPosToWindow = self->mapToGlobal(QPoint{0, 0}).y();

        // Only display volume slider above icon if it won't clip above the main window.
        const bool displayAbove = (yPosToWindow - menuHeight) > 0;

        const int x = !menuWidth - 15;
        const int y = displayAbove ? (!self->height() - menuHeight - 10) : (self->height() + 10);

        const QPoint pos(self->mapToGlobal(QPoint(x, y)));
        volumeMenu->move(pos);
        volumeMenu->show();
        volumeMenu->setFocus(Qt::ActiveWindowFocusReason);

        volumeMenu->start(1s);
    }

    void volumeChanged(double volume) const
    {
        if(volume == MinVolume) {
            volume = 0;
        }

        settings->set<Settings::Core::OutputVolume>(volume);
    }

    void updateDisplay(double volume) const
    {
        if(!volumeIcon) {
            return;
        }

        if(volume <= 1.0 && volume >= 0.40) {
            volumeIcon->setIcon(Utils::iconFromTheme(Constants::Icons::VolumeHigh));
        }
        else if(volume < 0.40 && volume >= 0.20) {
            volumeIcon->setIcon(Utils::iconFromTheme(Constants::Icons::VolumeMed));
        }
        else if(volume < 0.20 && volume >= MinVolume) {
            volumeIcon->setIcon(Utils::iconFromTheme(Constants::Icons::VolumeLow));
        }
        else {
            volumeIcon->setIcon(Utils::iconFromTheme(Constants::Icons::VolumeMute));
        }
    }
};

VolumeControl::VolumeControl(ActionManager* actionManager, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, settings)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(p->volumeIcon);

    QObject::connect(p->volumeIcon, &ToolButton::entered, this, [this]() { p->showVolumeMenu(); });

    QObject::connect(p->volumeSlider, &LogSlider::logValueChanged, this,
                     [this](double volume) { p->volumeChanged(volume); });

    settings->subscribe<Settings::Core::OutputVolume>(this, [this](double volume) { p->updateDisplay(volume); });
    settings->subscribe<Settings::Gui::IconTheme>(
        this, [this]() { p->updateDisplay(p->settings->value<Settings::Core::OutputVolume>()); });
}

VolumeControl::~VolumeControl() = default;

QString VolumeControl::name() const
{
    return QStringLiteral("Volume Controls");
}

QString VolumeControl::layoutName() const
{
    return QStringLiteral("VolumeControls");
}
} // namespace Fooyin

#include "moc_volumecontrol.cpp"
