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

#include <QObject>
#include <QTimer>

namespace Fy::Core::Engine::FFmpeg {
class Codec;

class EngineWorker : public QObject
{
    Q_OBJECT

public:
    explicit EngineWorker(QObject* parent = nullptr);

    void setPaused(bool isPaused);

public slots:
    virtual void reset();
    virtual void kill();

signals:
    void atEnd();

protected:
    QTimer* timer();

    virtual bool canDoNextStep() const;
    virtual int timerInterval() const;
    void scheduleNextStep(bool immediate = true);
    virtual void doNextStep() = 0;

    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] bool isAtEnd() const;

    void setAtEnd(bool isAtEnd);

private:
    QTimer* m_timer;
    std::atomic<bool> m_paused;
    bool m_atEnd;
};
} // namespace Fy::Core::Engine::FFmpeg
