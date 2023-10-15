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

#include "volumecontrol.h"

#include <core/coresettings.h>
#include <core/player/playermanager.h>
#include <gui/guiconstants.h>
#include <utils/comboicon.h>
#include <utils/hovermenu.h>
#include <utils/logslider.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>
#include <QLabel>

constexpr double MinVolume = 0.01;
constexpr QSize LabelSize  = {20, 20};

namespace Fy::Gui::Widgets {
struct VolumeControl::Private : QObject
{
    VolumeControl* self;
    Utils::SettingsManager* settings;
    Utils::ComboIcon* volumeIcon;
    double prevValue{-1.0};

    Private(VolumeControl* self, Utils::SettingsManager* settings)
        : self{self}
        , settings{settings}
        , volumeIcon{new Utils::ComboIcon(Constants::Icons::VolumeMute, self)}
    {
        settings->subscribe<Core::Settings::OutputVolume>(this, &VolumeControl::Private::updateDisplay);
    }

    void showVolumeMenu() const
    {
        auto* volumeMenu = new Utils::HoverMenu(self);
        volumeMenu->setAttribute(Qt::WA_DeleteOnClose);

        auto* volumeSlider = new Utils::LogSlider(Qt::Vertical, self);
        auto* volumeLayout = new QVBoxLayout(volumeMenu);
        volumeLayout->addWidget(volumeSlider);

        volumeSlider->setMinimumHeight(100);
        volumeSlider->setRange(MinVolume, 1.0);
        volumeSlider->setNaturalValue(settings->value<Core::Settings::OutputVolume>());

        connect(volumeSlider, &Utils::LogSlider::logValueChanged, this, &VolumeControl::Private::volumeChanged);
        settings->subscribe<Core::Settings::OutputVolume>(volumeSlider, &Utils::LogSlider::setNaturalValue);

        const int menuWidth  = volumeMenu->sizeHint().width();
        const int menuHeight = volumeMenu->sizeHint().height();

        const int yPosToWindow = self->parentWidget()->mapToParent(QPoint(0, 0)).y();

        // Only display volume slider above icon if it won't clip above the main window.
        const bool displayAbove = (yPosToWindow - menuHeight) > 0;

        const int x = !menuWidth - 15;
        const int y = displayAbove ? (!self->height() - menuHeight - 10) : (self->height() + 10);

        const QPoint pos{self->mapToGlobal(QPoint(x, y))};

        volumeMenu->move(pos);
        volumeMenu->show();
    }

    void volumeChanged(double volume) const
    {
        if(volume == MinVolume) {
            volume = 0;
        }

        settings->set<Core::Settings::OutputVolume>(volume);
    }

    void mute()
    {
        const double volume = settings->value<Core::Settings::OutputVolume>();

        if(volume != 0) {
            prevValue = volume;
            settings->set<Core::Settings::OutputVolume>(0.0);
        }
        else {
            settings->set<Core::Settings::OutputVolume>(prevValue < 0 ? 1 : prevValue);
        }
    }

    void updateDisplay(double volume) const
    {
        if(!volumeIcon) {
            return;
        }

        if(volume <= 1.0 && volume >= 0.40) {
            volumeIcon->setIcon(Constants::Icons::VolumeHigh);
        }
        else if(volume < 0.40 && volume >= 0.20) {
            volumeIcon->setIcon(Constants::Icons::VolumeMed);
        }
        else if(volume < 0.20 && volume >= MinVolume) {
            volumeIcon->setIcon(Constants::Icons::VolumeLow);
        }
        else {
            volumeIcon->setIcon(Constants::Icons::VolumeMute);
        }
    }
};

VolumeControl::VolumeControl(Utils::SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, settings)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 5, 0);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setSpacing(10);

    p->volumeIcon->addIcon(Constants::Icons::VolumeLow);
    p->volumeIcon->addIcon(Constants::Icons::VolumeMed);
    p->volumeIcon->addIcon(Constants::Icons::VolumeHigh);

    layout->addWidget(p->volumeIcon, 0, Qt::AlignRight | Qt::AlignVCenter);

    p->volumeIcon->setMaximumSize(LabelSize);

    p->updateDisplay(settings->value<Core::Settings::OutputVolume>());

    connect(p->volumeIcon, &Utils::ComboIcon::entered, p.get(), &VolumeControl::Private::showVolumeMenu);
    connect(p->volumeIcon, &Utils::ComboIcon::clicked, p.get(), &VolumeControl::Private::mute);
}

VolumeControl::~VolumeControl() = default;
} // namespace Fy::Gui::Widgets
