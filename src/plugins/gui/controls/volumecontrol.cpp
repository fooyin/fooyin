/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "gui/widgets/comboicon.h"
#include "gui/widgets/hovermenu.h"
#include "gui/widgets/slider.h"

#include <QHBoxLayout>
#include <core/constants.h>

namespace Gui::Widgets {
struct VolumeControl::Private
{
    QHBoxLayout* layout;

    ComboIcon* volumeIcon;

    Slider* volumeSlider;
    QHBoxLayout* volumeLayout;
    HoverMenu* volumeMenu;

    QSize labelSize{20, 20};
    int prevValue{0};
};

VolumeControl::VolumeControl(QWidget* parent)
    : QWidget(parent)
    , p(std::make_unique<Private>())
{
    setupUi();

    connect(p->volumeSlider, &QSlider::valueChanged, this, &VolumeControl::updateVolume);
    connect(p->volumeIcon, &ComboIcon::entered, this, &VolumeControl::showVolumeMenu);
    connect(p->volumeIcon, &ComboIcon::clicked, this, &VolumeControl::mute);
}

VolumeControl::~VolumeControl()
{
    delete p->volumeMenu;
}

void VolumeControl::setupUi()
{
    p->layout = new QHBoxLayout(this);
    p->layout->setContentsMargins(0, 0, 5, 0);
    p->layout->setSizeConstraint(QLayout::SetFixedSize);

    p->layout->setSpacing(10);

    p->volumeMenu   = new HoverMenu(this);
    p->volumeLayout = new QHBoxLayout(p->volumeMenu);

    p->volumeIcon = new ComboIcon(Core::Constants::Icons::VolumeMute, this);
    p->volumeIcon->addPixmap(Core::Constants::Icons::VolumeMin);
    p->volumeIcon->addPixmap(Core::Constants::Icons::VolumeLow);
    p->volumeIcon->addPixmap(Core::Constants::Icons::VolumeMed);
    p->volumeIcon->addPixmap(Core::Constants::Icons::VolumeMax);

    p->volumeSlider = new Slider(Qt::Vertical, this);
    p->volumeLayout->addWidget(p->volumeSlider);

    p->layout->addWidget(p->volumeIcon, 0, Qt::AlignRight | Qt::AlignVCenter);

    p->volumeSlider->setMaximum(100);
    p->volumeSlider->setValue(100);

    p->volumeIcon->setMaximumSize(p->labelSize);

    p->volumeIcon->setIcon(Core::Constants::Icons::VolumeMax);
}

void VolumeControl::updateVolume(double value)
{
    const double vol = value;
    emit volumeChanged(vol);

    if(vol <= 100 && vol >= 75) {
        p->volumeIcon->setIcon(Core::Constants::Icons::VolumeMax);
    }
    else if(vol < 75 && vol >= 50) {
        p->volumeIcon->setIcon(Core::Constants::Icons::VolumeMed);
    }
    else if(vol < 50 && vol >= 25) {
        p->volumeIcon->setIcon(Core::Constants::Icons::VolumeLow);
    }
    else if(vol < 25 && vol >= 1) {
        p->volumeIcon->setIcon(Core::Constants::Icons::VolumeMin);
    }
    else {
        p->volumeIcon->setIcon(Core::Constants::Icons::VolumeMute);
    }
}

void VolumeControl::mute()
{
    if(p->volumeSlider->value() != 0) {
        p->prevValue = p->volumeSlider->value();
        p->volumeSlider->setValue(0);
        return updateVolume(0);
    }
    p->volumeSlider->setValue(p->prevValue);
    return updateVolume(p->prevValue);
}

void VolumeControl::showVolumeMenu()
{
    const int menuWidth  = p->volumeMenu->sizeHint().width();
    const int menuHeight = p->volumeMenu->sizeHint().height();

    const int yPosToWindow = this->parentWidget()->mapToParent(QPoint(0, 0)).y();

    // Only display volume slider above icon if it won't clip above the main window.
    const bool displayAbove = (yPosToWindow - menuHeight) > 0;

    const int x = !menuWidth - 15;
    const int y = displayAbove ? (!this->height() - menuHeight - 10) : (this->height() + 10);

    const QPoint pos(mapToGlobal(QPoint(x, y)));
    p->volumeMenu->move(pos);
    p->volumeMenu->show();
}
} // namespace Gui::Widgets
