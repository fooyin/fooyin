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

#include <utils/id.h>

#include <QBasicTimer>
#include <QObject>

#include <map>

class QTimerEvent;

namespace Fooyin::GlobalHotkeys {
class GlobalShortcutRepeater : public QObject
{
    Q_OBJECT

public:
    explicit GlobalShortcutRepeater(QObject* parent = nullptr);
    GlobalShortcutRepeater(int initialDelay, int repeatInterval, QObject* parent = nullptr);

    void press(const QString& shortcutId, const Id& commandId);
    void release(const QString& shortcutId);
    void clear();

Q_SIGNALS:
    void activated(const Fooyin::Id& commandId);

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    struct RepeatState
    {
        explicit RepeatState(const Id& id)
            : commandId{id}
        { }

        Id commandId;
        QBasicTimer timer;
        bool repeating{false};
    };

    int m_initialDelay;
    int m_repeatInterval;
    std::map<QString, RepeatState> m_states;
};
} // namespace Fooyin::GlobalHotkeys
