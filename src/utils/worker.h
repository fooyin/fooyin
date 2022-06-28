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

#pragma once

#include <QObject>

class Worker : public QObject
{
public:
    enum State
    {
        IDLE,
        RUNNING
    };

    Worker(QObject* parent = nullptr)
        : QObject(parent){};
    ~Worker() override = default;

    virtual void stopThread() = 0;

    [[nodiscard]] State state() const
    {
        return m_state;
    };

    void setState(State state)
    {
        m_state = state;
    };

    [[nodiscard]] bool isRunning()
    {
        return m_state == State::RUNNING;
    };

private:
    State m_state{IDLE};
};
