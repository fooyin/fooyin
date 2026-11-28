/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "styleanimation.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QEvent>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTimerEvent>

#include <cmath>
#include <ranges>

namespace {
bool supportsTransitions(const QWidget* widget)
{
    return qobject_cast<const QAbstractButton*>(widget) || qobject_cast<const QAbstractSpinBox*>(widget)
        || qobject_cast<const QComboBox*>(widget) || qobject_cast<const QLineEdit*>(widget)
        || qobject_cast<const QTextEdit*>(widget) || qobject_cast<const QPlainTextEdit*>(widget)
        || qobject_cast<const QGroupBox*>(widget);
}
} // namespace

namespace Fooyin::Style {
StateAnimation::StateAnimation(QObject* parent, int duration)
    : QObject{parent}
    , m_duration{std::clamp(duration, 0, 400)}
{
    m_elapsed.start();
}

QColor StateAnimation::colour(const Transition& transition, qint64 now) const
{
    if(!transition.running || m_duration == 0) {
        return transition.target;
    }

    const float remaining
        = 1.0F - std::clamp(static_cast<float>(now - transition.started) / static_cast<float>(m_duration), 0.0F, 1.0F);
    const float progress  = 1.0F - (remaining * remaining * remaining);
    const float fromAlpha = transition.from.alphaF();
    const float toAlpha   = transition.target.alphaF();
    const float alpha     = std::lerp(fromAlpha, toAlpha, progress);
    if(alpha <= 0.0F) {
        return QColor{Qt::transparent};
    }

    // Interpolate premultiplied colours so transparent tints retain their hue while fading
    return QColor::fromRgbF(
        std::lerp(transition.from.redF() * fromAlpha, transition.target.redF() * toAlpha, progress) / alpha,
        std::lerp(transition.from.greenF() * fromAlpha, transition.target.greenF() * toAlpha, progress) / alpha,
        std::lerp(transition.from.blueF() * fromAlpha, transition.target.blueF() * toAlpha, progress) / alpha, alpha);
}

QBrush StateAnimation::brush(const QStyleOption& option, const QWidget* widget, TransitionChannel channel,
                             const QBrush& target)
{
    if(m_duration == 0 || !widget || !widget->isVisible() || !supportsTransitions(widget)
       || (option.state & QStyle::State_Item)) {
        return target;
    }

    auto& entry  = m_entries[widget];
    entry.widget = const_cast<QWidget*>(widget);

    auto& transition = entry.colours.at(static_cast<size_t>(channel));
    // Gradient backgrounds keep their original stops and geometry
    if(target.style() != Qt::SolidPattern) {
        transition.reset();
        return target;
    }

    const auto now = m_elapsed.elapsed();
    if(!transition || !(option.state & QStyle::State_Enabled)) {
        transition = Transition{.from = target.color(), .target = target.color(), .started = now, .running = false};
        return target;
    }

    if(target.color() != transition->target) {
        transition
            = Transition{.from = colour(*transition, now), .target = target.color(), .started = now, .running = true};
        if(!m_timer.isActive()) {
            m_timer.start(16, this);
        }
    }

    return QBrush{colour(*transition, now)};
}

void StateAnimation::setDuration(int duration)
{
    duration = std::clamp(duration, 0, 1000);
    if(std::exchange(m_duration, duration) == duration) {
        return;
    }

    m_timer.stop();

    for(const auto& entry : m_entries | std::views::values) {
        if(entry.widget) {
            entry.widget->update();
        }
    }
    m_entries.clear();
}

void StateAnimation::polish(QWidget* widget)
{
    if(supportsTransitions(widget)) {
        widget->installEventFilter(this);
    }
}

void StateAnimation::remove(QWidget* widget)
{
    m_entries.erase(widget);
    widget->removeEventFilter(this);
    if(m_entries.empty()) {
        m_timer.stop();
    }
}

bool StateAnimation::eventFilter(QObject* watched, QEvent* event)
{
    switch(event->type()) {
        case QEvent::Hide:
        case QEvent::Destroy:
        case QEvent::EnabledChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            m_entries.erase(watched);
            if(m_entries.empty()) {
                m_timer.stop();
            }
            break;
        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

void StateAnimation::timerEvent(QTimerEvent* event)
{
    if(event->timerId() != m_timer.timerId()) {
        return;
    }

    const auto now = m_elapsed.elapsed();
    bool running{false};
    for(auto it = m_entries.begin(); it != m_entries.end();) {
        auto& [widget, colours] = it->second;
        if(!widget || !widget->isVisible()) {
            it = m_entries.erase(it);
            continue;
        }

        bool update{false};
        for(auto& transition : colours) {
            if(transition && transition->running) {
                update              = true;
                transition->running = now - transition->started < m_duration;
                running |= transition->running;
            }
        }
        if(update) {
            widget->update();
        }
        ++it;
    }
    if(!running) {
        m_timer.stop();
    }
}
} // namespace Fooyin::Style
