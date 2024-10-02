/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <QBasicTimer>
#include <QObject>

namespace Fooyin {
class FYUTILS_EXPORT SignalThrottler : public QObject
{
    Q_OBJECT

public:
    explicit SignalThrottler(QObject* parent = nullptr);
    ~SignalThrottler() override;

    [[nodiscard]] bool isActive() const;

    [[nodiscard]] int timeout() const;
    void setTimeout(int timeout);
    void setTimeout(std::chrono::milliseconds timeout);

    [[nodiscard]] Qt::TimerType timerType() const;
    void setTimerType(Qt::TimerType timerType);

signals:
    void triggered();
    void timeoutChanged(int timeout);
    void timerTypeChanged(Qt::TimerType timerType);

public slots:
    void throttle();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void maybeEmitTriggered();
    void emitTriggered();

    QBasicTimer m_timer;
    int m_timeout;
    Qt::TimerType m_timerType;
    bool m_pendingEmit;
};
} // namespace Fooyin
