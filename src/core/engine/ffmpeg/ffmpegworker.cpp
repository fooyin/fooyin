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

#include "ffmpegworker.h"

#include <QDebug>

namespace Fy::Core::Engine::FFmpeg {
EngineWorker::EngineWorker(QObject* parent)
    : QObject{parent}
    , m_timer{nullptr}
    , m_paused{false}
    , m_atEnd{false}
{ }

void EngineWorker::setPaused(bool isPaused)
{
    m_paused = isPaused;
}

void EngineWorker::reset()
{
    setPaused(true);
    m_atEnd = false;
}

void EngineWorker::kill()
{
    setPaused(true);
    m_atEnd = false;
    if(m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
}

QTimer* EngineWorker::timer()
{
    if(!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        m_timer->setSingleShot(true);
        QObject::connect(m_timer, &QTimer::timeout, this, [this]() {
            if(canDoNextStep()) {
                doNextStep();
            }
        });
    }
    return m_timer;
}

bool EngineWorker::canDoNextStep() const
{
    return !m_paused && !m_atEnd;
}

int EngineWorker::timerInterval() const
{
    return 0;
}

void EngineWorker::scheduleNextStep(bool immediate)
{
    if(canDoNextStep()) {
        const auto interval = timerInterval();
        if(interval == 0 && immediate) {
            timer()->stop();
            doNextStep();
        }
        else {
            timer()->start(interval);
        }
    }
    else {
        timer()->stop();
    }
}

bool EngineWorker::isPaused() const
{
    return m_paused;
}

bool EngineWorker::isAtEnd() const
{
    return m_atEnd;
}

void EngineWorker::setAtEnd(bool isAtEnd)
{
    if(std::exchange(m_atEnd, isAtEnd) != isAtEnd) {
        emit atEnd();
    }
}
} // namespace Fy::Core::Engine::FFmpeg
