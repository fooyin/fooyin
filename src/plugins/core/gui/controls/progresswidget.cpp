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

#include "progresswidget.h"

#include "core/coresettings.h"
#include "core/gui/widgets/clickablelabel.h"
#include "core/gui/widgets/slider.h"
#include "core/library/models/track.h"

#include <QHBoxLayout>
#include <QSlider>
#include <pluginsystem/pluginmanager.h>
#include <utils/utils.h>

namespace Core::Widgets {
struct ProgressWidget::Private
{
    QHBoxLayout* layout;

    Slider* slider;
    ClickableLabel* elapsed;
    ClickableLabel* total;

    int max{0};

    SettingsManager* settings{PluginSystem::object<SettingsManager>()};
};

ProgressWidget::ProgressWidget(QWidget* parent)
    : QWidget(parent)
    , p(std::make_unique<Private>())
{
    setupUi();

    connect(p->total, &ClickableLabel::clicked, this, &ProgressWidget::toggleRemaining);
    connect(p->slider, &Slider::sliderReleased, this, &ProgressWidget::sliderDropped);
}

ProgressWidget::~ProgressWidget() = default;

void ProgressWidget::setupUi()
{
    p->layout = new QHBoxLayout(this);

    p->layout->setContentsMargins(0, 0, 0, 0);

    p->slider  = new Slider(Qt::Horizontal, this);
    p->elapsed = new ClickableLabel(this);
    p->total   = new ClickableLabel(this);

    p->slider->setFocusPolicy(Qt::NoFocus);

    p->elapsed->setText("00:00");
    p->total->setText("00:00");

    p->layout->addWidget(p->elapsed, 0, Qt::AlignVCenter | Qt::AlignLeft);
    p->layout->addWidget(p->slider, 0, Qt::AlignVCenter);
    p->layout->addWidget(p->total, 0, Qt::AlignVCenter | Qt::AlignLeft);

    setEnabled(false);
}

void ProgressWidget::changeTrack(Track* track)
{
    reset();
    p->max = static_cast<int>(track->duration());
    p->slider->setMaximum(p->max);
    auto elapsed = p->settings->value<Settings::ElapsedTotal>();
    p->total->setText(elapsed ? "-" + Utils::msToString(p->max) : Utils::msToString(p->max));
    setEnabled(true);
}

void ProgressWidget::setCurrentPosition(int ms)
{
    if(!p->slider->isSliderDown()) {
        p->slider->setValue(ms);
        updateTime(ms);
    }
}

void ProgressWidget::updateTime(int elapsed)
{
    const int secs = elapsed / 1000;
    const int max  = p->max / 1000;

    p->elapsed->setText(Utils::secsToString(secs));

    if(p->settings->value<Settings::ElapsedTotal>()) {
        const int remaining = max - secs;
        p->total->setText("-" + Utils::secsToString(remaining));
    }
    else {
        if(!p->slider->isEnabled()) {
            p->total->setText(Utils::msToString(max));
        }
    }
}

void ProgressWidget::reset()
{
    auto elapsed = p->settings->value<Settings::ElapsedTotal>();
    p->elapsed->setText("00:00");
    p->total->setText(elapsed ? "-00:00" : "00:00");
    p->slider->setValue(0);
    p->max = 0;
}

void ProgressWidget::stateChanged(Player::PlayState state)
{
    switch(state) {
        case(Player::PlayState::Stopped):
            reset();
            return setEnabled(false);
        case(Player::PlayState::Playing):
            return setEnabled(true);
        case(Player::PlayState::Paused):
            return;
    }
}

void ProgressWidget::toggleRemaining()
{
    if(p->settings->value<Settings::ElapsedTotal>()) {
        p->settings->set<Settings::ElapsedTotal>(false);
        p->total->setText(Utils::msToString(p->max));
    }
    else {
        p->settings->set<Settings::ElapsedTotal>(true);
    }
}

void ProgressWidget::sliderDropped()
{
    const auto pos = p->slider->value();
    emit movedSlider(pos);
}
} // namespace Core::Widgets
