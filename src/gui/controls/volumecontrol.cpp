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

#include "gui/guiconstants.h"

#include <utils/comboicon.h>
#include <utils/hovermenu.h>
#include <utils/slider.h>

#include <QHBoxLayout>

namespace Gui::Widgets {
VolumeControl::VolumeControl(QWidget* parent)
    : QWidget{parent}
    , m_layout{new QHBoxLayout(this)}
    , m_volumeIcon{new Utils::ComboIcon(Constants::Icons::VolumeMute, this)}
    , m_volumeSlider{new Utils::Slider(Qt::Vertical, this)}
    , m_volumeLayout{new QHBoxLayout()}
    , m_volumeMenu{new Utils::HoverMenu(this)}
    , m_labelSize{20, 20}
    , m_prevValue{0}
{
    m_volumeMenu->setLayout(m_volumeLayout);

    setupUi();

    connect(m_volumeSlider, &QSlider::valueChanged, this, &VolumeControl::updateVolume);
    connect(m_volumeIcon, &Utils::ComboIcon::entered, this, &VolumeControl::showVolumeMenu);
    connect(m_volumeIcon, &Utils::ComboIcon::clicked, this, &VolumeControl::mute);
}

VolumeControl::~VolumeControl()
{
    delete m_volumeMenu;
}

void VolumeControl::setupUi()
{
    m_layout->setContentsMargins(0, 0, 5, 0);
    m_layout->setSizeConstraint(QLayout::SetFixedSize);

    m_layout->setSpacing(10);

    m_volumeIcon->addPixmap(Constants::Icons::VolumeMin);
    m_volumeIcon->addPixmap(Constants::Icons::VolumeLow);
    m_volumeIcon->addPixmap(Constants::Icons::VolumeMed);
    m_volumeIcon->addPixmap(Constants::Icons::VolumeMax);

    m_volumeLayout->addWidget(m_volumeSlider);

    m_layout->addWidget(m_volumeIcon, 0, Qt::AlignRight | Qt::AlignVCenter);

    m_volumeSlider->setMaximum(100);
    m_volumeSlider->setValue(100);

    m_volumeIcon->setMaximumSize(m_labelSize);

    m_volumeIcon->setIcon(Constants::Icons::VolumeMax);
}

void VolumeControl::updateVolume(double value)
{
    const double vol = value;
    emit volumeChanged(vol);

    if(vol <= 100 && vol >= 75) {
        m_volumeIcon->setIcon(Constants::Icons::VolumeMax);
    }
    else if(vol < 75 && vol >= 50) {
        m_volumeIcon->setIcon(Constants::Icons::VolumeMed);
    }
    else if(vol < 50 && vol >= 25) {
        m_volumeIcon->setIcon(Constants::Icons::VolumeLow);
    }
    else if(vol < 25 && vol >= 1) {
        m_volumeIcon->setIcon(Constants::Icons::VolumeMin);
    }
    else {
        m_volumeIcon->setIcon(Constants::Icons::VolumeMute);
    }
}

void VolumeControl::mute()
{
    if(m_volumeSlider->value() != 0) {
        m_prevValue = m_volumeSlider->value();
        m_volumeSlider->setValue(0);
        return updateVolume(0);
    }
    m_volumeSlider->setValue(m_prevValue);
    return updateVolume(m_prevValue);
}

void VolumeControl::showVolumeMenu()
{
    const int menuWidth  = m_volumeMenu->sizeHint().width();
    const int menuHeight = m_volumeMenu->sizeHint().height();

    const int yPosToWindow = this->parentWidget()->mapToParent(QPoint(0, 0)).y();

    // Only display volume slider above icon if it won't clip above the main window.
    const bool displayAbove = (yPosToWindow - menuHeight) > 0;

    const int x = !menuWidth - 15;
    const int y = displayAbove ? (!this->height() - menuHeight - 10) : (this->height() + 10);

    const QPoint pos(mapToGlobal(QPoint(x, y)));
    m_volumeMenu->move(pos);
    m_volumeMenu->show();
}
} // namespace Gui::Widgets
