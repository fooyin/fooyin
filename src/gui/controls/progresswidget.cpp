/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "internalguisettings.h"

#include <core/player/playermanager.h>
#include <core/track.h>
#include <utils/clickablelabel.h>
#include <utils/settings/settingsmanager.h>
#include <utils/slider.h>
#include <utils/utils.h>

#include <QHBoxLayout>
#include <QSlider>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
ProgressWidget::ProgressWidget(PlayerManager* playerManager, SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_playerManager{playerManager}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(this)}
    , m_slider{new Slider(Qt::Horizontal, this)}
    , m_elapsed{new ClickableLabel(this)}
    , m_total{new ClickableLabel(this)}
    , m_max{0}
    , m_elapsedTotal{settings->value<Settings::Gui::Internal::ElapsedTotal>()}
{
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_layout->addWidget(m_elapsed, 0, Qt::AlignVCenter | Qt::AlignLeft);
    m_layout->addWidget(m_slider, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_total, 0, Qt::AlignVCenter | Qt::AlignLeft);

    setEnabled(m_playerManager->currentTrack().isValid());

    QObject::connect(m_total, &ClickableLabel::clicked, this, &ProgressWidget::toggleRemaining);
    QObject::connect(m_slider, &Slider::sliderReleased, this, &ProgressWidget::sliderDropped);

    QObject::connect(m_playerManager, &PlayerManager::playStateChanged, this, &ProgressWidget::stateChanged);
    QObject::connect(m_playerManager, &PlayerManager::currentTrackChanged, this, &ProgressWidget::changeTrack);
    QObject::connect(m_playerManager, &PlayerManager::positionChanged, this, &ProgressWidget::setCurrentPosition);
    QObject::connect(this, &ProgressWidget::movedSlider, m_playerManager, &PlayerManager::changePosition);

    changeTrack(m_playerManager->currentTrack());
}

void ProgressWidget::reset()
{
    m_elapsed->setText(u"00:00"_s);
    m_total->setText(m_elapsedTotal ? u"-00:00"_s : u"00:00"_s);
    m_slider->setValue(0);
    m_max = 0;
}

void ProgressWidget::changeTrack(const Track& track)
{
    reset();

    if(track.isValid()) {
        m_max = track.duration();
        m_slider->setMaximum(static_cast<int>(m_max));
        const QString totalText = m_elapsedTotal ? u"-"_s : u""_s + Utils::msToString(m_max);
        m_total->setText(totalText);
    }
}

void ProgressWidget::setCurrentPosition(uint64_t ms)
{
    if(!m_slider->isSliderDown()) {
        m_slider->setValue(static_cast<int>(ms));
        updateTime(ms);
    }
}

void ProgressWidget::updateTime(uint64_t elapsed)
{
    m_elapsed->setText(Utils::msToString(elapsed));

    if(m_elapsedTotal) {
        const int remaining = static_cast<int>(m_max - elapsed);
        m_total->setText("-" + Utils::msToString(remaining < 0 ? 0 : remaining));
    }
    else {
        if(!m_slider->isEnabled()) {
            m_total->setText(Utils::msToString(m_max));
        }
    }
}

void ProgressWidget::stateChanged(PlayState state)
{
    switch(state) {
        case(PlayState::Stopped): {
            reset();
            setEnabled(false);
            break;
        }
        case(PlayState::Playing): {
            setEnabled(true);
            break;
        }
        case(PlayState::Paused): {
            return;
        }
    }
}

void ProgressWidget::toggleRemaining()
{
    if(m_elapsedTotal) {
        m_settings->set<Settings::Gui::Internal::ElapsedTotal>(false);
        m_total->setText(Utils::msToString(m_max));
    }
    else {
        m_settings->set<Settings::Gui::Internal::ElapsedTotal>(true);
    }
    m_elapsedTotal = !m_elapsedTotal;
}

void ProgressWidget::sliderDropped()
{
    const auto pos = m_slider->value();
    emit movedSlider(pos);
}
} // namespace Fooyin

#include "moc_progresswidget.cpp"
