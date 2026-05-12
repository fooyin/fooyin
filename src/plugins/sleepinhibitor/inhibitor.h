/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
 * Copyright © 2026, Gustav Oechler <gustavoechler@gmail.com>
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

#include <QObject>

namespace Fooyin::SleepInhibitor {
class InhibitorPrivate : public QObject
{
    Q_OBJECT

public:
    enum class State : uint8_t
    {
        Initializing,
        Error,
        Uninhibited,
        Inhibited,
    };

    explicit InhibitorPrivate(QObject* parent = nullptr);

    [[nodiscard]] State state() const
    {
        return m_state;
    }

    void setState(State state)
    {
        m_state = state;
    }

    virtual void inhibitSleep()   = 0;
    virtual void uninhibitSleep() = 0;

private:
    State m_state{State::Initializing};
};

class Inhibitor : public QObject
{
    Q_OBJECT

public:
    explicit Inhibitor(QObject* parent = nullptr);

    void inhibitSleep();
    void uninhibitSleep();

private:
    InhibitorPrivate* p;
};
} // namespace Fooyin::SleepInhibitor
