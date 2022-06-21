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

#include "gui/widgets/clickablelabel.h"
#include "utils/utils.h"

#include <QHBoxLayout>
#include <QSlider>

struct VolumeControl::Private
{
    QHBoxLayout* layout;
    ClickableLabel* mute;
    //    QSlider* volumeSlider;

    int prevValue{0};
};

VolumeControl::VolumeControl(QWidget* parent)
    : QWidget(parent)
{
    p = std::make_unique<Private>();

    setupUi();

    //    connect(p->volumeSlider, &QSlider::valueChanged, this,
    //            &VolumeControl::updateVolume);
    connect(p->mute, &ClickableLabel::clicked, this, &VolumeControl::mute);
}

VolumeControl::~VolumeControl() = default;

void VolumeControl::setupUi()
{
    p->layout = new QHBoxLayout(this);
    setLayout(p->layout);
    p->layout->setContentsMargins(0, 0, 5, 0);
    p->layout->setSizeConstraint(QLayout::SetFixedSize);

    p->layout->setSpacing(10);

    p->mute = new ClickableLabel(this);
    //    p->volumeSlider = new QSlider(Qt::Horizontal, this);

    //    p->volumeSlider->setFocusPolicy(Qt::NoFocus);

    //    p->layout->addWidget(p->volumeSlider, 0, Qt::AlignRight | Qt::AlignVCenter);
    p->layout->addWidget(p->mute, 0, Qt::AlignRight | Qt::AlignVCenter);

    //    p->volumeSlider->setMaximum(100);
    //    p->volumeSlider->setValue(100);

    QFont font = QFont("Guifx v2 Transports", 12);
    setFont(font);

    p->mute->setText("$");

    Util::setMinimumWidth(p->mute, "$");
}

void VolumeControl::updateVolume(double value)
{
    double vol = value;
    emit volumeChanged(vol);

    if(vol <= 100 && vol >= 66) {
        p->mute->setText("$");
    }
    else if(vol < 66 && vol >= 33) {
        p->mute->setText("#");
    }
    else if(vol < 33 && vol > 0) {
        p->mute->setText("@");
    }
    else {
        p->mute->setText("!");
    }
}

void VolumeControl::mute()
{
    //    if (p->volumeSlider->value() != 0)
    //    {
    //        p->prevValue = p->volumeSlider->value();
    //        p->volumeSlider->setValue(0);
    //        return updateVolume(0);
    //    }
    //    p->volumeSlider->setValue(p->prevValue);
    p->prevValue = 100;
    return updateVolume(p->prevValue);
}
