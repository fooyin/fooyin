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

#pragma once

#include "fyutils_export.h"

#include <QObject>

namespace Fooyin {
class FYUTILS_EXPORT Worker : public QObject
{
    Q_OBJECT

public:
    enum State
    {
        Idle = 0,
        Running,
        Paused
    };

    explicit Worker(QObject* parent = nullptr);

    virtual void stopThread();
    virtual void pauseThread();
    virtual void closeThread();

    [[nodiscard]] State state() const;
    void setState(State state);

    [[nodiscard]] bool mayRun() const;

signals:
    void finished();

private:
    std::atomic<State> m_state;
};
} // namespace Fooyin
