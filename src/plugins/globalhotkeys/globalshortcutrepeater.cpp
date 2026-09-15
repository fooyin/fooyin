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

#include "globalshortcutrepeater.h"

#include <QGuiApplication>
#include <QStyleHints>
#include <QTimerEvent>

#include <chrono>
#include <ranges>

namespace Fooyin::GlobalHotkeys {
namespace {
int repeatInterval()
{
    const QStyleHints* styleHints = QGuiApplication::styleHints();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const qreal repeatRate = styleHints->keyboardAutoRepeatRateF();
#else
    const auto repeatRate = static_cast<qreal>(styleHints->keyboardAutoRepeatRate());
#endif
    return repeatRate > 0 ? std::max(qRound(1000.0 / repeatRate), 1) : 0;
}

void startBasicTimer(QBasicTimer& timer, int interval, QObject* receiver)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    timer.start(std::chrono::milliseconds{interval}, receiver);
#else
    timer.start(interval, receiver);
#endif
}

bool isTimerEvent(const QBasicTimer& timer, const QTimerEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    return timer.id() == event->id();
#else
    return timer.timerId() == event->timerId();
#endif
}
} // namespace

GlobalShortcutRepeater::GlobalShortcutRepeater(QObject* parent)
    : GlobalShortcutRepeater{std::max(QGuiApplication::styleHints()->keyboardInputInterval(), 1), repeatInterval(),
                             parent}
{ }

GlobalShortcutRepeater::GlobalShortcutRepeater(int initialDelay, int repeatInterval, QObject* parent)
    : QObject{parent}
    , m_initialDelay{std::max(initialDelay, 1)}
    , m_repeatInterval{std::max(repeatInterval, 0)}
{ }

void GlobalShortcutRepeater::press(const QString& shortcutId, const Id& commandId)
{
    const auto [state, inserted] = m_states.try_emplace(shortcutId, commandId);
    if(!inserted) {
        return;
    }

    if(m_repeatInterval > 0) {
        startBasicTimer(state->second.timer, m_initialDelay, this);
    }

    Q_EMIT activated(commandId);
}

void GlobalShortcutRepeater::release(const QString& shortcutId)
{
    m_states.erase(shortcutId);
}

void GlobalShortcutRepeater::clear()
{
    m_states.clear();
}

void GlobalShortcutRepeater::timerEvent(QTimerEvent* event)
{
    for(auto& state : m_states | std::views::values) {
        if(!isTimerEvent(state.timer, event)) {
            continue;
        }

        if(!state.repeating) {
            state.repeating = true;
            startBasicTimer(state.timer, m_repeatInterval, this);
        }

        const Id commandId = state.commandId;
        Q_EMIT activated(commandId);
        return;
    }

    QObject::timerEvent(event);
}
} // namespace Fooyin::GlobalHotkeys
