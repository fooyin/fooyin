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

#pragma once

#include <QBasicTimer>
#include <QBrush>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QStyleOption>

#include <array>
#include <optional>
#include <unordered_map>

namespace Fooyin::Style {
enum class TransitionChannel : uint8_t
{
    Background = 0,
    Tint,
    Border,
    Foreground,
    Icon,
    Halo,
    MenuBackground,
    MenuTint,
    MenuBorder,
    Count,
};

class StateAnimation : public QObject
{
    Q_OBJECT

public:
    StateAnimation(QObject* parent, int duration);

    QBrush brush(const QStyleOption& option, const QWidget* widget, TransitionChannel channel, const QBrush& target);
    void setDuration(int duration);
    void polish(QWidget* widget);
    void remove(QWidget* widget);

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    struct Transition
    {
        QColor from;
        QColor target;
        int64_t started;
        bool running;
    };

    struct Entry
    {
        QPointer<QWidget> widget;
        std::array<std::optional<Transition>, static_cast<size_t>(TransitionChannel::Count)> colours;
    };

    QColor colour(const Transition& transition, qint64 now) const;

    int m_duration;
    QBasicTimer m_timer;
    QElapsedTimer m_elapsed;
    std::unordered_map<const QObject*, Entry> m_entries;
};
} // namespace Fooyin::Style
