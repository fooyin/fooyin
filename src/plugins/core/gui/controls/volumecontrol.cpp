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

#include "core/constants.h"
#include "core/gui/widgets/clickablelabel.h"
#include "core/gui/widgets/hovermenu.h"
#include "core/gui/widgets/slider.h"

#include <QHBoxLayout>
#include <utils/utils.h>

struct VolumeControl::Private
{
    QHBoxLayout* layout;
    ClickableLabel* mute;
    Slider* volumeSlider;
    QHBoxLayout* volumeLayout;
    HoverMenu* volumeMenu;

    int prevValue{0};
};

VolumeControl::VolumeControl(QWidget* parent)
    : QWidget(parent)
    , p(std::make_unique<Private>())
{
    setupUi();

    connect(p->volumeSlider, &QSlider::valueChanged, this, &VolumeControl::updateVolume);
    connect(p->mute, &ClickableLabel::entered, this, &VolumeControl::showVolumeMenu);
    connect(p->mute, &ClickableLabel::clicked, this, &VolumeControl::mute);
}

VolumeControl::~VolumeControl()
{
    delete p->volumeMenu;
};

void VolumeControl::setupUi()
{
    p->layout = new QHBoxLayout(this);
    p->layout->setContentsMargins(0, 0, 5, 0);
    p->layout->setSizeConstraint(QLayout::SetFixedSize);

    p->layout->setSpacing(10);

    p->volumeMenu = new HoverMenu(this);
    p->volumeLayout = new QHBoxLayout(p->volumeMenu);

    p->mute = new ClickableLabel(this);
    p->volumeSlider = new Slider(Qt::Vertical, this);
    p->volumeLayout->addWidget(p->volumeSlider);

    p->layout->addWidget(p->mute, 0, Qt::AlignRight | Qt::AlignVCenter);

    p->volumeSlider->setMaximum(100);
    p->volumeSlider->setValue(100);

    QFont font = QFont(Core::Constants::IconFont, 12);
    setFont(font);

    p->mute->setText(Core::Constants::Icons::VolumeMax);

    Util::setMinimumWidth(p->mute, Core::Constants::Icons::VolumeMax);
}

void VolumeControl::updateVolume(double value)
{
    double vol = value;
    emit volumeChanged(vol);

    if(vol <= 100 && vol >= 66) {
        p->mute->setText(Core::Constants::Icons::VolumeMax);
    }
    else if(vol < 66 && vol >= 33) {
        p->mute->setText(Core::Constants::Icons::VolumeMid);
    }
    else if(vol < 33 && vol > 0) {
        p->mute->setText(Core::Constants::Icons::VolumeMin);
    }
    else {
        p->mute->setText(Core::Constants::Icons::VolumeMute);
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
    const int menuWidth = p->volumeMenu->sizeHint().width();
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
