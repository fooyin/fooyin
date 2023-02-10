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

#include "progresswidget.h"

#include "gui/guisettings.h"

#include <core/models/track.h>
#include <core/player/playermanager.h>
#include <utils/clickablelabel.h>
#include <utils/slider.h>
#include <utils/utils.h>

#include <QHBoxLayout>
#include <QSlider>

namespace Gui::Widgets {
ProgressWidget::ProgressWidget(Core::SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(this)}
    , m_slider{new Utils::Slider(Qt::Horizontal, this)}
    , m_elapsed{new Utils::ClickableLabel(this)}
    , m_total{new Utils::ClickableLabel(this)}
    , m_max{0}
{
    setupUi();

    connect(m_total, &Utils::ClickableLabel::clicked, this, &ProgressWidget::toggleRemaining);
    connect(m_slider, &Utils::Slider::sliderReleased, this, &ProgressWidget::sliderDropped);
}

void ProgressWidget::setupUi()
{
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_slider->setFocusPolicy(Qt::NoFocus);

    m_elapsed->setText("00:00");
    m_total->setText("00:00");

    m_layout->addWidget(m_elapsed, 0, Qt::AlignVCenter | Qt::AlignLeft);
    m_layout->addWidget(m_slider, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_total, 0, Qt::AlignVCenter | Qt::AlignLeft);

    setEnabled(false);
}

void ProgressWidget::changeTrack(Core::Track* track)
{
    reset();
    m_max = static_cast<int>(track->duration());
    m_slider->setMaximum(m_max);
    auto elapsed = m_settings->value<Settings::ElapsedTotal>();
    m_total->setText(elapsed ? "-" + Utils::msToString(m_max) : Utils::msToString(m_max));
    setEnabled(true);
}

void ProgressWidget::setCurrentPosition(int ms)
{
    if(!m_slider->isSliderDown()) {
        m_slider->setValue(ms);
        updateTime(ms);
    }
}

void ProgressWidget::updateTime(int elapsed)
{
    const int secs = elapsed / 1000;
    const int max  = m_max / 1000;

    m_elapsed->setText(Utils::secsToString(secs));

    if(m_settings->value<Settings::ElapsedTotal>()) {
        const int remaining = max - secs;
        m_total->setText("-" + Utils::secsToString(remaining));
    }
    else {
        if(!m_slider->isEnabled()) {
            m_total->setText(Utils::msToString(max));
        }
    }
}

void ProgressWidget::reset()
{
    auto elapsed = m_settings->value<Settings::ElapsedTotal>();
    m_elapsed->setText("00:00");
    m_total->setText(elapsed ? "-00:00" : "00:00");
    m_slider->setValue(0);
    m_max = 0;
}

void ProgressWidget::stateChanged(Core::Player::PlayState state)
{
    switch(state) {
        case(Core::Player::Stopped):
            reset();
            return setEnabled(false);
        case(Core::Player::Playing):
            return setEnabled(true);
        case(Core::Player::Paused):
            return;
    }
}

void ProgressWidget::toggleRemaining()
{
    if(m_settings->value<Settings::ElapsedTotal>()) {
        m_settings->set<Settings::ElapsedTotal>(false);
        m_total->setText(Utils::msToString(m_max));
    }
    else {
        m_settings->set<Settings::ElapsedTotal>(true);
    }
}

void ProgressWidget::sliderDropped()
{
    const auto pos = m_slider->value();
    emit movedSlider(pos);
}
} // namespace Gui::Widgets
