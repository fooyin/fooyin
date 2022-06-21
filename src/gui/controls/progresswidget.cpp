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

#include "gui/widgets/clickablelabel.h"
#include "gui/widgets/slider.h"
#include "models/track.h"
#include "utils/settings.h"
#include "utils/utils.h"

#include <QHBoxLayout>
#include <QSlider>

struct ProgressWidget::Private
{
    QHBoxLayout* layout;

    Slider* slider;
    ClickableLabel* elapsed;
    ClickableLabel* total;

    int max{0};

    Settings* settings{Settings::instance()};
};

ProgressWidget::ProgressWidget(QWidget* parent)
    : QWidget(parent)
{
    p = std::make_unique<Private>();
    setupUi();

    connect(p->total, &ClickableLabel::clicked, this, &ProgressWidget::toggleRemaining);
    connect(p->slider, &Slider::sliderReleased, this, &ProgressWidget::sliderDropped);
}

ProgressWidget::~ProgressWidget() = default;

void ProgressWidget::setupUi()
{
    p->layout = new QHBoxLayout(this);
    setLayout(p->layout);

    p->layout->setContentsMargins(0, 0, 0, 0);

    p->slider = new Slider(Qt::Horizontal, this);
    p->elapsed = new ClickableLabel(this);
    p->total = new ClickableLabel(this);

    p->slider->setFocusPolicy(Qt::NoFocus);

    p->total->setMaximumWidth(35);
    p->total->setMinimumWidth(35);

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
    auto elapsed = p->settings->value(Settings::Setting::ElapsedTotal).toBool();
    p->total->setText(elapsed ? "-" + Util::msToString(p->max) : Util::msToString(p->max));
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
    auto* settings = Settings::instance();
    int secs = elapsed / 1000;
    int max = p->max / 1000;

    p->elapsed->setText(Util::secsToString(secs));

    if(settings->value(Settings::Setting::ElapsedTotal).toBool()) {
        int remaining = max - secs;
        p->total->setText("-" + Util::secsToString(remaining));
    }
    else {
        if(!p->slider->isEnabled()) {
            p->total->setText(Util::msToString(max));
        }
    }
}

void ProgressWidget::reset()
{
    auto* settings = Settings::instance();
    auto elapsed = settings->value(Settings::Setting::ElapsedTotal).toBool();
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
    auto* settings = Settings::instance();
    if(settings->value(Settings::Setting::ElapsedTotal).toBool()) {
        settings->set(Settings::Setting::ElapsedTotal, false);
        p->total->setText(Util::msToString(p->max));
    }
    else {
        settings->set(Settings::Setting::ElapsedTotal, true);
    }
}

void ProgressWidget::sliderDropped()
{
    const auto pos = p->slider->value();
    emit movedSlider(pos);
}
