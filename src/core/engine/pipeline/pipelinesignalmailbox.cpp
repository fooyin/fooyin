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

#include "pipelinesignalmailbox.h"

#include <QCoreApplication>
#include <QEvent>
#include <QObject>

namespace Fooyin {
PipelineSignalMailbox::PipelineSignalMailbox()
    : m_pendingSignalBits{0}
    , m_signalWakePosted{false}
    , m_signalWakeTarget{nullptr}
    , m_signalWakeEventType{signalWakeEventType()}
{ }

void PipelineSignalMailbox::setWakeTarget(QObject* target, int wakeEventType)
{
    m_signalWakeTarget.store(target, std::memory_order_release);
    m_signalWakeEventType.store(wakeEventType > 0 ? wakeEventType : signalWakeEventType(), std::memory_order_release);
}

uint32_t PipelineSignalMailbox::drain()
{
    const uint32_t signalBits = m_pendingSignalBits.exchange(0, std::memory_order_acq_rel);

    // Allow one new wake after this drain
    m_signalWakePosted.store(false, std::memory_order_release);

    if(m_pendingSignalBits.load(std::memory_order_acquire) != 0) {
        postWakeIfNeeded();
    }

    return signalBits;
}

void PipelineSignalMailbox::clear(uint32_t signalMask)
{
    m_pendingSignalBits.fetch_and(~signalMask, std::memory_order_release);
}

void PipelineSignalMailbox::mark(uint32_t signalMask)
{
    m_pendingSignalBits.fetch_or(signalMask, std::memory_order_release);
    postWakeIfNeeded();
}

void PipelineSignalMailbox::reset()
{
    m_pendingSignalBits.store(0, std::memory_order_release);
    m_signalWakePosted.store(false, std::memory_order_release);
}

int PipelineSignalMailbox::signalWakeEventType()
{
    static const int eventType = QEvent::registerEventType();
    return eventType;
}

void PipelineSignalMailbox::postWakeIfNeeded()
{
    if(m_signalWakePosted.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    auto* target = m_signalWakeTarget.load(std::memory_order_acquire);
    if(!target) {
        m_signalWakePosted.store(false, std::memory_order_release);
        return;
    }

    const int wakeEventType = m_signalWakeEventType.load(std::memory_order_acquire);
    QCoreApplication::postEvent(target, new QEvent(static_cast<QEvent::Type>(wakeEventType)));
}
} // namespace Fooyin
